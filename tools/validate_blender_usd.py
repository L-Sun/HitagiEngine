#!/usr/bin/env python3
"""Validate the Hitagi Blender/USD authoring subset for ASCII USDA files."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

TOON_SHADER_ID = "HitagiToonSurface"
SURFACE_SHADER_IDS = {"UsdPreviewSurface", TOON_SHADER_ID}
TOON_INPUTS = {
    "baseColor",
    "baseTexture",
    "shadeColor1",
    "shadeColor2",
    "shadeRamp",
    "shadowThreshold",
    "shadowSoftness",
    "rimColor",
    "rimWidth",
    "rimIntensity",
    "outlineColor",
    "outlineWidth",
    "normalTexture",
    "faceShadowMask",
    "matcapTexture",
    "specularThreshold",
    "specularIntensity",
    "emissionColor",
}
TOON_REQUIRED_VALUE_INPUTS = {
    "baseColor",
    "shadeColor1",
    "shadeColor2",
    "shadowThreshold",
    "shadowSoftness",
    "rimColor",
    "rimWidth",
    "rimIntensity",
    "outlineColor",
    "outlineWidth",
    "specularThreshold",
    "specularIntensity",
    "emissionColor",
}
TOON_ZERO_TO_ONE_INPUTS = {"shadowThreshold", "specularThreshold"}
TOON_NON_NEGATIVE_INPUTS = {
    "shadowSoftness",
    "rimWidth",
    "rimIntensity",
    "outlineWidth",
    "specularIntensity",
}


@dataclass(frozen=True)
class PrimBlock:
    kind: str
    name: str
    path: str
    body: str


def strip_comments(text: str) -> str:
    lines: list[str] = []
    for line in text.splitlines():
        if line.lstrip().startswith("#"):
            lines.append("")
        else:
            lines.append(line)
    return "\n".join(lines)


def find_matching_brace(text: str, open_index: int) -> int:
    depth = 0
    for index in range(open_index, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError(f"unmatched brace at byte {open_index}")


def collect_prims(text: str) -> dict[str, PrimBlock]:
    prims: dict[str, PrimBlock] = {}
    pattern = re.compile(r'\bdef\s+([A-Za-z_][\w]*)\s+"([^"]+)"')

    def walk(start: int, end: int, parent_path: str) -> None:
        pos = start
        while True:
            match = pattern.search(text, pos, end)
            if match is None:
                break

            open_brace = text.find("{", match.end(), end)
            if open_brace == -1:
                break

            close_brace = find_matching_brace(text, open_brace)
            if close_brace >= end:
                pos = match.end()
                continue

            kind, name = match.group(1), match.group(2)
            path = f"{parent_path}/{name}" if parent_path else f"/{name}"
            body = text[open_brace + 1 : close_brace]
            prims[path] = PrimBlock(kind=kind, name=name, path=path, body=body)
            walk(open_brace + 1, close_brace, path)
            pos = close_brace + 1

    walk(0, len(text), "")
    return prims


def interpolation_for(body: str, declaration: str) -> str | None:
    start = body.find(declaration)
    if start == -1:
        return None

    segment = body[start : start + 1000]
    match = re.search(r'interpolation\s*=\s*"([^"]+)"', segment)
    return match.group(1) if match else None


def resolve_asset_path(usd_path: Path, asset_value: str) -> Path:
    asset_path = Path(asset_value.replace("\\", "/"))
    if asset_path.is_absolute():
        return asset_path
    return (usd_path.parent / asset_path).resolve()


def shader_input_names(body: str) -> set[str]:
    return set(re.findall(r"\binputs:([A-Za-z_][\w]*)", body))


def float_input(body: str, name: str) -> float | None:
    match = re.search(rf"\bfloat\s+inputs:{re.escape(name)}\s*=\s*([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)", body)
    return float(match.group(1)) if match else None


def validate_toon_shader(shader: PrimBlock) -> list[str]:
    errors: list[str] = []
    input_names = shader_input_names(shader.body)

    for required in sorted(TOON_REQUIRED_VALUE_INPUTS):
        if required not in input_names:
            errors.append(f"{shader.path}: missing required Toon input inputs:{required}")

    for input_name in sorted(input_names):
        if input_name not in TOON_INPUTS:
            errors.append(f"{shader.path}: unsupported Toon input inputs:{input_name}")

    for input_name in sorted(TOON_ZERO_TO_ONE_INPUTS):
        value = float_input(shader.body, input_name)
        if value is not None and not 0.0 <= value <= 1.0:
            errors.append(f"{shader.path}: inputs:{input_name} must be in [0, 1], got {value:g}")

    for input_name in sorted(TOON_NON_NEGATIVE_INPUTS):
        value = float_input(shader.body, input_name)
        if value is not None and value < 0.0:
            errors.append(f"{shader.path}: inputs:{input_name} must be non-negative, got {value:g}")

    return errors


def validate(usd_path: Path, require_vertex_color: bool) -> list[str]:
    text = strip_comments(usd_path.read_text(encoding="utf-8"))
    prims = collect_prims(text)
    errors: list[str] = []

    material_paths = {path for path, prim in prims.items() if prim.kind == "Material"}
    shader_paths = {path for path, prim in prims.items() if prim.kind == "Shader"}
    meshes = [prim for prim in prims.values() if prim.kind == "Mesh"]

    if not meshes:
        errors.append("no Mesh prims found")

    for mesh in meshes:
        binding = re.search(r"rel\s+material:binding\s*=\s*<([^>]+)>", mesh.body)
        if binding is None:
            errors.append(f"{mesh.path}: missing rel material:binding")
        else:
            material_path = binding.group(1)
            if material_path not in material_paths:
                errors.append(f"{mesh.path}: bound material does not exist: {material_path}")

        if "primvars:st" not in mesh.body:
            errors.append(f"{mesh.path}: missing primary UV primvars:st")
        else:
            uv_interp = interpolation_for(mesh.body, "primvars:st")
            if uv_interp not in {"vertex", "faceVarying"}:
                errors.append(
                    f"{mesh.path}: primvars:st interpolation must be vertex or faceVarying, got {uv_interp or 'none'}"
                )

        has_vertex_color = "primvars:displayColor" in mesh.body
        if require_vertex_color and not has_vertex_color:
            errors.append(f"{mesh.path}: missing required primvars:displayColor")
        if has_vertex_color:
            color_interp = interpolation_for(mesh.body, "primvars:displayColor")
            if color_interp not in {"constant", "uniform", "vertex", "faceVarying"}:
                errors.append(
                    f"{mesh.path}: primvars:displayColor interpolation must be constant, uniform, vertex, or faceVarying"
                )

        if "normal3f[] normals" in mesh.body:
            normal_interp = interpolation_for(mesh.body, "normal3f[] normals")
            if normal_interp not in {"vertex", "faceVarying"}:
                errors.append(
                    f"{mesh.path}: normals interpolation must be vertex or faceVarying, got {normal_interp or 'none'}"
                )

    for material_path in material_paths:
        material = prims[material_path]
        surface = re.search(r"outputs:surface\.connect\s*=\s*<([^>]+)>", material.body)
        if surface is None:
            errors.append(f"{material_path}: missing outputs:surface.connect")
            continue

        shader_path = surface.group(1).split(".outputs:")[0]
        shader = prims.get(shader_path)
        if shader is None or shader.path not in shader_paths:
            errors.append(f"{material_path}: surface target is not a Shader prim: {shader_path}")
            continue

        shader_id = re.search(r'info:id\s*=\s*"([^"]+)"', shader.body)
        if shader_id is None or shader_id.group(1) not in SURFACE_SHADER_IDS:
            actual = shader_id.group(1) if shader_id else "none"
            errors.append(f"{material_path}: surface shader must be UsdPreviewSurface or {TOON_SHADER_ID}, got {actual}")
            continue

        if shader_id.group(1) == TOON_SHADER_ID:
            errors.extend(validate_toon_shader(shader))

    for shader in (prim for prim in prims.values() if prim.kind == "Shader"):
        shader_id = re.search(r'info:id\s*=\s*"([^"]+)"', shader.body)
        if shader_id is None or shader_id.group(1) != "UsdUVTexture":
            continue

        asset = re.search(r"asset\s+inputs:file\s*=\s*@([^@]+)@", shader.body)
        if asset is None:
            errors.append(f"{shader.path}: UsdUVTexture missing asset inputs:file")
            continue

        texture_path = resolve_asset_path(usd_path, asset.group(1))
        if not texture_path.exists():
            errors.append(f"{shader.path}: texture does not exist: {asset.group(1)}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("usd", type=Path, help="USDA file to validate")
    parser.add_argument(
        "--require-vertex-color",
        action="store_true",
        help="Require primvars:displayColor on every mesh",
    )
    args = parser.parse_args()

    usd_path = args.usd.resolve()
    if not usd_path.exists():
        print(f"error: USD file does not exist: {args.usd}", file=sys.stderr)
        return 2
    if usd_path.suffix.lower() != ".usda":
        print("error: static validator currently supports ASCII .usda files only", file=sys.stderr)
        return 2

    try:
        errors = validate(usd_path, args.require_vertex_color)
    except Exception as exc:
        print(f"error: failed to parse {args.usd}: {exc}", file=sys.stderr)
        return 2

    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"ok: {args.usd}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
