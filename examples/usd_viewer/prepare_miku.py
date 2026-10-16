"""Download Animasa's Miku PMD from the pinned Three.js sample and write static USD.

The model has its own terms, not Three.js's MIT license or Shino's CC0 license.
Keep the original notices alongside the locally downloaded/converted assets.
Only this sample's diffuse materials and bind-pose geometry are converted;
MMD animation, physics, morphs, sphere maps and toon textures are not imported.
"""
import hashlib
import json
from pathlib import Path
import struct
import urllib.request

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "assets/anime_viewer/miku"
REVISION = "2898f5b1ba10b1e94174c0a62d072f5f7b80442c"
BASE = f"https://raw.githubusercontent.com/mrdoob/three.js/{REVISION}/examples/models/mmd/"
FILES = {
    "miku_v2.pmd": "45a5a2fb897266a1b371b30cf53ed9435d375ddea9fce7f095bba2739b9bdae5",
    "eyeM2.bmp": "a318f2d5bd22e3ac3a5b8cab4021fd68e25042e7730e8c04720cee68e3548565",
}


def array(values):
    if values.ndim == 1:
        return "[" + ",".join(str(int(v)) for v in values) + "]"
    return "[" + ",".join("(" + ",".join(f"{float(v):.7g}" for v in row) + ")" for row in values) + "]"


def prepare():
    DEST.mkdir(parents=True, exist_ok=True)
    for name, digest in FILES.items():
        target = DEST / name
        if not target.exists():
            urllib.request.urlretrieve(BASE + "miku/" + name, target)
        if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Unexpected checksum: {name}")
    for source, name in [("LICENSE", "LICENSE.three-mmd.txt"), ("Readme.txt", "Readme.three-mmd.txt"),
                         ("miku/readme.txt", "readme.txt"), ("miku/readme_miku_v2.txt", "readme_miku_v2.txt")]:
        if not (DEST / name).exists():
            urllib.request.urlretrieve(BASE + source, DEST / name)

    raw = (DEST / "miku_v2.pmd").read_bytes()
    if raw[:3] != b"Pmd" or struct.unpack_from("<f", raw, 3)[0] != 1:
        raise ValueError("Expected PMD 1.0")
    model_name = raw[7:27].split(b"\0")[0].decode("cp932")
    comment = raw[27:283].split(b"\0")[0].decode("cp932")
    count = struct.unpack_from("<I", raw, 283)[0]
    vertices = np.ndarray((count, 8), dtype="<f4", buffer=raw, offset=287, strides=(38, 4)).copy()
    bone_indices = np.ndarray((count, 2), dtype="<u2", buffer=raw, offset=319, strides=(38, 2))
    bone_weight = np.ndarray((count,), dtype="u1", buffer=raw, offset=323, strides=(38,)).astype(float) / 100
    head_weight = (bone_indices[:, 0] == 3) * bone_weight + (bone_indices[:, 1] == 3) * (1-bone_weight)
    # Convert MMD's left-handed coordinates to right-handed Y-up USD, then turn
    # 180 degrees around Y to face the viewer's default camera at negative Z.
    vertices[:, [0, 3]] *= -1
    vertices[:, 7] = 1 - vertices[:, 7]  # USD's bottom-left UV origin.
    pos, normals, uv = vertices[:, :3], vertices[:, 3:6], vertices[:, 6:8]
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-8)
    offset = 287 + count * 38
    num_indices = struct.unpack_from("<I", raw, offset)[0]
    indices = np.frombuffer(raw, dtype="<u2", count=num_indices, offset=offset + 4).reshape(-1, 3)[:, [0, 2, 1]].reshape(-1)
    if int(indices.max()) >= count:
        raise ValueError("Invalid vertex index")
    offset += 4 + num_indices * 2
    num_materials = struct.unpack_from("<I", raw, offset)[0]
    offset += 4
    materials = [struct.unpack_from("<4ff3f3fBBI20s", raw, offset + i * 70) for i in range(num_materials)]
    bones_offset = offset + num_materials * 70 + 2
    if raw[bones_offset+3*39:bones_offset+3*39+20].split(b"\0")[0].decode("cp932") != "頭":
        raise ValueError("Pinned Miku head bone layout changed")
    if sum(m[13] for m in materials) != num_indices:
        raise ValueError("Material groups do not cover the mesh")

    lines = ['#usda 1.0', '(\n defaultPrim = "World"\n upAxis = "Y"\n metersPerUnit = 0.08\n)',
             'def Xform "World" {', 'def Scope "Looks" {']
    # These semantic slots belong to this pinned asset, not an engine heuristic.
    # Skin also covers the body: split its head triangles into a face material.
    material_kinds = [0, 3, 1, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 2, 4]
    output_materials = materials + [materials[2], materials[15]]
    for i, m in enumerate(output_materials):
        texture = m[14].split(b"\0")[0].decode("cp932")
        lines += [f'def Material "M{i}" {{', f'token outputs:surface.connect = </World/Looks/M{i}/Surface.outputs:surface>',
                  'def Shader "Surface" {', 'uniform token info:id = "UsdPreviewSurface"', 'token outputs:surface',
                  'float inputs:roughness = 0.8', 'float inputs:opacityThreshold = 0',
                  f'float inputs:animeMaterial = {material_kinds[i]}',
                  'float4 inputs:animeHead = (0, 18.1, 0, 1.45)']
        if texture:
            if texture != "eyeM2.bmp":
                raise ValueError(f"Unsupported PMD texture: {texture}")
            pixels = np.asarray(Image.open(DEST / texture).convert("RGBA")).astype(float) * np.array(m[:4])
            Image.fromarray(np.clip(pixels, 0, 255).astype(np.uint8)).save(DEST / f"material_{i}.png")
            lines += [f'color3f inputs:diffuseColor.connect = </World/Looks/M{i}/Texture.outputs:rgb>', '}',
                      'def Shader "Texture" {', 'uniform token info:id = "UsdUVTexture"',
                      f'asset inputs:file = @material_{i}.png@',
                      f'float2 inputs:st.connect = </World/Looks/M{i}/UV.outputs:result>',
                      'float3 outputs:rgb', '}', 'def Shader "UV" {',
                      'uniform token info:id = "UsdPrimvarReader_float2"', 'string inputs:varname = "st"',
                      'float2 outputs:result', '}']
        else:
            lines += [f'color3f inputs:diffuseColor = ({m[0]}, {m[1]}, {m[2]})', '}']
        lines += ['}']
    lines += ['}']
    start = 0
    groups = []
    for i, m in enumerate(materials):
        triangles = indices[start:start + m[13]].reshape(-1, 3)
        start += m[13]
        if i in (2, 15):
            # No geometry is changed; separate face skin and eye white from body.
            head = head_weight[triangles].mean(axis=1) > 0.5 if i == 2 else pos[triangles, 1].min(axis=1) > 17.2
            groups += [(i, triangles[~head]), (17 if i == 2 else 18, triangles[head])]
        else:
            groups.append((i, triangles))
    for i, triangles in groups:
        if not len(triangles):
            continue
        selected, local_indices = np.unique(triangles.reshape(-1), return_inverse=True)
        lines += [f'def Mesh "Mesh_{i}" (prepend apiSchemas = ["MaterialBindingAPI"]) {{',
                  'uniform token subdivisionScheme = "none"', 'bool doubleSided = true',
                  f'point3f[] points = {array(pos[selected])}',
                  f'normal3f[] normals = {array(normals[selected])} (interpolation = "vertex")',
                  f'texCoord2f[] primvars:st = {array(uv[selected])} (interpolation = "vertex")',
                  f'int[] faceVertexCounts = {array(np.full(len(triangles), 3))}',
                  f'int[] faceVertexIndices = {array(local_indices)}',
                  f'rel material:binding = </World/Looks/M{i}>', '}']
    lines += ['}']
    (DEST / "miku.usda").write_text("\n".join(lines), encoding="utf-8")
    (DEST / "provenance.json").write_text(json.dumps({
        "source": BASE + "miku/miku_v2.pmd", "sha256": FILES,
        "model_name": model_name, "original_comment": comment,
        "author": "Animasa / あにまさ", "character_copyright": "CRYPTON FUTURE MEDIA, INC",
        "terms": "See original readme_miku_v2.txt and Readme.three-mmd.txt; not MIT or CC0.",
        "vertices": count, "triangles": num_indices // 3, "source_materials": num_materials,
        "materials": len(output_materials), "material_kinds": material_kinds,
        "conversion": "Static bind pose; reflected X and reversed winding; V-flipped UVs; diffuse materials only."
    }, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Prepared {num_indices // 3} triangles, {len(output_materials)} materials: {DEST / 'miku.usda'}")


if __name__ == "__main__":
    prepare()
