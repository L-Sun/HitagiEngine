#!/usr/bin/env python3
"""Hitagi Toon material authoring helper and Blender add-on skeleton.

The module is intentionally useful without Blender: CI can import/compile it and
can ask it to write a small USDA file that exercises the fixed
HitagiToonSurface schema. When loaded in Blender, it registers a conservative
material panel and an export operator that writes the same schema.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

try:
    import bpy  # type: ignore
    from bpy.props import BoolProperty, FloatProperty, FloatVectorProperty, PointerProperty, StringProperty  # type: ignore
    from bpy.types import Material, Operator, Panel, PropertyGroup  # type: ignore
except ModuleNotFoundError:  # pragma: no cover - exercised by plain Python checks.
    bpy = None
    BoolProperty = FloatProperty = FloatVectorProperty = PointerProperty = StringProperty = None
    Material = Operator = Panel = PropertyGroup = object


bl_info = {
    "name": "Hitagi Toon Authoring",
    "author": "HitagiEngine",
    "version": (0, 1, 0),
    "blender": (4, 0, 0),
    "location": "Material Properties > Hitagi Toon",
    "description": "Author and export fixed HitagiToonSurface USD materials.",
    "category": "Import-Export",
}

TOON_SHADER_ID = "HitagiToonSurface"
TOON_INPUTS = (
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
    "outlineMask",
    "normalTexture",
    "faceShadowMask",
    "matcapTexture",
    "specularThreshold",
    "specularIntensity",
    "emissionColor",
)
VERTEX_COLOR_CHANNELS = {
    "displayColor": "RGB vertex tint, exported as primvars:displayColor.",
    "hitagi_shadow_mask": "Optional scalar occlusion/shadow bias stored in color.r.",
    "hitagi_outline_mask": "Optional scalar outline width multiplier stored in color.r.",
}
IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


@dataclass(frozen=True)
class TextureSlot:
    input_name: str
    shader_name: str
    path: str = ""
    output: str = "rgb"


@dataclass
class HitagiToonMaterial:
    name: str = "HitagiToonMaterial"
    base_color: tuple[float, float, float] = (1.0, 1.0, 1.0)
    shade_color_1: tuple[float, float, float] = (0.72, 0.72, 0.72)
    shade_color_2: tuple[float, float, float] = (0.45, 0.45, 0.45)
    shadow_threshold: float = 0.5
    shadow_softness: float = 0.05
    rim_color: tuple[float, float, float] = (1.0, 1.0, 1.0)
    rim_width: float = 0.35
    rim_intensity: float = 0.0
    outline_color: tuple[float, float, float] = (0.0, 0.0, 0.0)
    outline_width: float = 1.0
    specular_threshold: float = 0.8
    specular_intensity: float = 0.0
    emission_color: tuple[float, float, float] = (0.0, 0.0, 0.0)
    textures: list[TextureSlot] = field(default_factory=list)


@dataclass(frozen=True)
class MeshExportData:
    name: str = "Triangle"
    material_name: str = "HitagiToonMaterial"
    points: tuple[tuple[float, float, float], ...] = ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
    indices: tuple[int, ...] = (0, 1, 2)
    uvs: tuple[tuple[float, float], ...] = ((0.0, 0.0), (1.0, 0.0), (0.0, 1.0))
    colors: tuple[tuple[float, float, float], ...] = ((1.0, 1.0, 1.0), (1.0, 0.85, 0.75), (0.75, 0.85, 1.0))
    normals: tuple[tuple[float, float, float], ...] = ((0.0, 0.0, 1.0), (0.0, 0.0, 1.0), (0.0, 0.0, 1.0))


@dataclass(frozen=True)
class ExportIssue:
    severity: str
    message: str


def _fmt_tuple(values: Sequence[float]) -> str:
    return "(" + ", ".join(f"{value:.6g}" for value in values) + ")"


def _fmt_array(values: Iterable[Sequence[float]]) -> str:
    return "[" + ", ".join(_fmt_tuple(value) for value in values) + "]"


def _validate_identifier(kind: str, name: str, issues: list[ExportIssue]) -> None:
    if not IDENTIFIER_RE.fullmatch(name):
        issues.append(ExportIssue("error", f"{kind} name must be a valid USD identifier: {name!r}"))


def preflight_toon_export(materials: Sequence[HitagiToonMaterial], meshes: Sequence[MeshExportData], usd_path: Path) -> list[ExportIssue]:
    issues: list[ExportIssue] = []
    material_names = {material.name for material in materials}

    if not materials:
        issues.append(ExportIssue("error", "export has no Hitagi Toon materials"))
    if not meshes:
        issues.append(ExportIssue("error", "export has no meshes"))

    for material in materials:
        _validate_identifier("material", material.name, issues)
        for color_name in ("base_color", "shade_color_1", "shade_color_2", "rim_color", "outline_color", "emission_color"):
            color = getattr(material, color_name)
            if any(channel < 0.0 or channel > 1.0 for channel in color):
                issues.append(ExportIssue("error", f"{material.name}.{color_name} must be in [0, 1]"))
        for attr in ("shadow_threshold", "specular_threshold"):
            value = getattr(material, attr)
            if value < 0.0 or value > 1.0:
                issues.append(ExportIssue("error", f"{material.name}.{attr} must be in [0, 1]"))
        for attr in ("shadow_softness", "rim_width", "rim_intensity", "outline_width", "specular_intensity"):
            if getattr(material, attr) < 0.0:
                issues.append(ExportIssue("error", f"{material.name}.{attr} must be non-negative"))
        for texture in material.textures:
            if texture.input_name not in TOON_INPUTS:
                issues.append(ExportIssue("error", f"{material.name}: unknown Toon texture input {texture.input_name!r}"))
            _validate_identifier("texture shader", texture.shader_name, issues)
            if not texture.path:
                issues.append(ExportIssue("warning", f"{material.name}.{texture.input_name} has no texture path"))
                continue
            resolved = (usd_path.parent / texture.path).resolve()
            if not resolved.exists():
                issues.append(ExportIssue("error", f"{material.name}.{texture.input_name} missing texture: {texture.path}"))

    for mesh in meshes:
        _validate_identifier("mesh", mesh.name, issues)
        if mesh.material_name not in material_names:
            issues.append(ExportIssue("error", f"{mesh.name} references missing material {mesh.material_name!r}"))
        if not mesh.uvs:
            issues.append(ExportIssue("error", f"{mesh.name} is missing primary UV channel primvars:st"))
        if len(mesh.colors) not in (0, 1, len(mesh.points)):
            issues.append(ExportIssue("error", f"{mesh.name} vertex color count must be 1 or match point count"))
        if mesh.normals and len(mesh.normals) != len(mesh.points):
            issues.append(ExportIssue("error", f"{mesh.name} custom normal count must match point count"))

    return issues


def write_hitagi_toon_usda(path: Path, materials: Sequence[HitagiToonMaterial], meshes: Sequence[MeshExportData]) -> None:
    issues = preflight_toon_export(materials, meshes, path)
    errors = [issue for issue in issues if issue.severity == "error"]
    if errors:
        raise ValueError("; ".join(issue.message for issue in errors))

    material_by_name = {material.name: material for material in materials}
    lines: list[str] = [
        "#usda 1.0",
        "(",
        '    defaultPrim = "Root"',
        "    metersPerUnit = 1",
        '    upAxis = "Y"',
        ")",
        "",
        'def Xform "Root"',
        "{",
        '    def Scope "Materials"',
        "    {",
    ]

    for material in materials:
        lines.extend(_material_usda_lines(material))
    lines.extend(["    }", ""])

    for mesh in meshes:
        lines.extend(_mesh_usda_lines(mesh, material_by_name[mesh.material_name]))

    lines.append("}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _material_usda_lines(material: HitagiToonMaterial) -> list[str]:
    material_path = f"/Root/Materials/{material.name}"
    shader_path = f"{material_path}/ToonSurface"
    lines = [
        f'        def Material "{material.name}"',
        "        {",
        f"            token outputs:surface.connect = <{shader_path}.outputs:surface>",
        "",
        '            def Shader "ToonSurface"',
        "            {",
        f'                uniform token info:id = "{TOON_SHADER_ID}"',
        f"                color3f inputs:baseColor = {_fmt_tuple(material.base_color)}",
        f"                color3f inputs:shadeColor1 = {_fmt_tuple(material.shade_color_1)}",
        f"                color3f inputs:shadeColor2 = {_fmt_tuple(material.shade_color_2)}",
        f"                float inputs:shadowThreshold = {material.shadow_threshold:.6g}",
        f"                float inputs:shadowSoftness = {material.shadow_softness:.6g}",
        f"                color3f inputs:rimColor = {_fmt_tuple(material.rim_color)}",
        f"                float inputs:rimWidth = {material.rim_width:.6g}",
        f"                float inputs:rimIntensity = {material.rim_intensity:.6g}",
        f"                color3f inputs:outlineColor = {_fmt_tuple(material.outline_color)}",
        f"                float inputs:outlineWidth = {material.outline_width:.6g}",
        f"                float inputs:specularThreshold = {material.specular_threshold:.6g}",
        f"                float inputs:specularIntensity = {material.specular_intensity:.6g}",
        f"                color3f inputs:emissionColor = {_fmt_tuple(material.emission_color)}",
    ]
    for texture in material.textures:
        usd_type = "float" if texture.output in {"r", "g", "b", "a"} else "color3f"
        lines.append(f"                {usd_type} inputs:{texture.input_name}.connect = <{material_path}/{texture.shader_name}.outputs:{texture.output}>")
    lines.extend(["                token outputs:surface", "            }", ""])
    for texture in material.textures:
        usd_type = "float" if texture.output in {"r", "g", "b", "a"} else "color3f"
        lines.extend(
            [
                f'            def Shader "{texture.shader_name}"',
                "            {",
                '                uniform token info:id = "UsdUVTexture"',
                f"                asset inputs:file = @{texture.path}@",
                f"                {usd_type} outputs:{texture.output}",
                "            }",
                "",
            ]
        )
    lines.extend(["        }", ""])
    return lines


def _mesh_usda_lines(mesh: MeshExportData, material: HitagiToonMaterial) -> list[str]:
    return [
        f'    def Mesh "{mesh.name}" (',
        '        prepend apiSchemas = ["MaterialBindingAPI"]',
        "    )",
        "    {",
        f"        rel material:binding = </Root/Materials/{material.name}>",
        "        int[] faceVertexCounts = [3]",
        "        int[] faceVertexIndices = [" + ", ".join(str(index) for index in mesh.indices) + "]",
        f"        point3f[] points = {_fmt_array(mesh.points)}",
        f"        normal3f[] normals = {_fmt_array(mesh.normals)} (",
        '            interpolation = "vertex"',
        "        )",
        f"        texCoord2f[] primvars:st = {_fmt_array(mesh.uvs)} (",
        '            interpolation = "vertex"',
        "        )",
        f"        color3f[] primvars:displayColor = {_fmt_array(mesh.colors)} (",
        '            interpolation = "vertex"',
        "        )",
        "    }",
        "",
    ]


def sample_toon_material() -> HitagiToonMaterial:
    return HitagiToonMaterial(
        name="HitagiToonMaterial",
        base_color=(0.92, 0.72, 0.58),
        shade_color_1=(0.55, 0.38, 0.32),
        shade_color_2=(0.25, 0.16, 0.15),
        shadow_threshold=0.42,
        shadow_softness=0.06,
        rim_color=(0.55, 0.78, 1.0),
        rim_width=0.28,
        rim_intensity=1.25,
        outline_color=(0.02, 0.018, 0.025),
        outline_width=1.75,
        specular_threshold=0.72,
        specular_intensity=0.32,
        emission_color=(0.04, 0.02, 0.01),
        textures=[
            TextureSlot("baseTexture", "BaseTexture", "./test.png", "rgb"),
            TextureSlot("shadeRamp", "ShadeRamp", "./test.png", "rgb"),
            TextureSlot("outlineMask", "OutlineMask", "./test.png", "r"),
            TextureSlot("normalTexture", "NormalTexture", "./test.png", "rgb"),
            TextureSlot("faceShadowMask", "FaceShadowMask", "./test.png", "r"),
            TextureSlot("matcapTexture", "MatcapTexture", "./test.png", "rgb"),
        ],
    )


if bpy is not None:

    def ensure_hitagi_toon_preview_node_group():
        """Create a lightweight Eevee/Cycles preview group for Hitagi Toon inputs."""
        group = bpy.data.node_groups.get("Hitagi Toon Preview")
        if group is None:
            group = bpy.data.node_groups.new("Hitagi Toon Preview", "ShaderNodeTree")

        group.nodes.clear()
        group.links.clear()

        group_input = group.nodes.new("NodeGroupInput")
        group_input.location = (-600, 0)
        group_output = group.nodes.new("NodeGroupOutput")
        group_output.location = (300, 0)
        principled = group.nodes.new("ShaderNodeBsdfPrincipled")
        principled.location = (0, 0)

        def socket(collection, name: str, socket_type: str):
            if hasattr(group, "interface"):
                try:
                    return group.interface.new_socket(name=name, in_out="INPUT", socket_type=socket_type)
                except TypeError:
                    pass
            if name not in collection:
                return collection.new(socket_type, name)
            return collection[name]

        socket(group.inputs, "Base Color", "NodeSocketColor")
        socket(group.inputs, "Shade Color 1", "NodeSocketColor")
        socket(group.inputs, "Shadow Threshold", "NodeSocketFloat")
        if hasattr(group, "interface"):
            try:
                group.interface.new_socket(name="BSDF", in_out="OUTPUT", socket_type="NodeSocketShader")
            except TypeError:
                pass
        elif "BSDF" not in group.outputs:
            group.outputs.new("NodeSocketShader", "BSDF")

        if "Base Color" in group_input.outputs and "Base Color" in principled.inputs:
            group.links.new(group_input.outputs["Base Color"], principled.inputs["Base Color"])
        if "BSDF" in principled.outputs and "BSDF" in group_output.inputs:
            group.links.new(principled.outputs["BSDF"], group_output.inputs["BSDF"])
        return group

    class HitagiToonProperties(PropertyGroup):
        enabled: BoolProperty(name="Use Hitagi Toon", default=True)
        base_color: FloatVectorProperty(name="Base Color", subtype="COLOR", size=3, min=0.0, max=1.0, default=(1.0, 1.0, 1.0))
        shade_color_1: FloatVectorProperty(name="Shade Color 1", subtype="COLOR", size=3, min=0.0, max=1.0, default=(0.72, 0.72, 0.72))
        shade_color_2: FloatVectorProperty(name="Shade Color 2", subtype="COLOR", size=3, min=0.0, max=1.0, default=(0.45, 0.45, 0.45))
        shadow_threshold: FloatProperty(name="Shadow Threshold", min=0.0, max=1.0, default=0.5)
        shadow_softness: FloatProperty(name="Shadow Softness", min=0.0, default=0.05)
        rim_color: FloatVectorProperty(name="Rim Color", subtype="COLOR", size=3, min=0.0, max=1.0, default=(1.0, 1.0, 1.0))
        rim_width: FloatProperty(name="Rim Width", min=0.0, default=0.35)
        rim_intensity: FloatProperty(name="Rim Intensity", min=0.0, default=0.0)
        outline_color: FloatVectorProperty(name="Outline Color", subtype="COLOR", size=3, min=0.0, max=1.0, default=(0.0, 0.0, 0.0))
        outline_width: FloatProperty(name="Outline Width", min=0.0, default=1.0)
        specular_threshold: FloatProperty(name="Specular Threshold", min=0.0, max=1.0, default=0.8)
        specular_intensity: FloatProperty(name="Specular Intensity", min=0.0, default=0.0)
        emission_color: FloatVectorProperty(name="Emission Color", subtype="COLOR", size=3, min=0.0, max=1.0, default=(0.0, 0.0, 0.0))
        base_texture: StringProperty(name="Base Texture", subtype="FILE_PATH", default="")
        shade_ramp: StringProperty(name="Shade Ramp", subtype="FILE_PATH", default="")
        normal_texture: StringProperty(name="Normal Texture", subtype="FILE_PATH", default="")
        outline_mask: StringProperty(name="Outline Mask", subtype="FILE_PATH", default="")
        face_shadow_mask: StringProperty(name="Face Shadow Mask", subtype="FILE_PATH", default="")
        matcap_texture: StringProperty(name="Matcap Texture", subtype="FILE_PATH", default="")


    class HITAGI_PT_toon_material(Panel):
        bl_label = "Hitagi Toon"
        bl_idname = "HITAGI_PT_toon_material"
        bl_space_type = "PROPERTIES"
        bl_region_type = "WINDOW"
        bl_context = "material"

        def draw(self, context):
            material = context.material
            layout = self.layout
            if material is None:
                layout.label(text="No active material")
                return
            props = material.hitagi_toon
            layout.prop(props, "enabled")
            layout.prop(props, "base_color")
            layout.prop(props, "shade_color_1")
            layout.prop(props, "shade_color_2")
            layout.prop(props, "shadow_threshold")
            layout.prop(props, "shadow_softness")
            layout.prop(props, "rim_color")
            layout.prop(props, "rim_width")
            layout.prop(props, "rim_intensity")
            layout.prop(props, "outline_color")
            layout.prop(props, "outline_width")
            layout.prop(props, "specular_threshold")
            layout.prop(props, "specular_intensity")
            layout.prop(props, "emission_color")
            layout.prop(props, "base_texture")
            layout.prop(props, "shade_ramp")
            layout.prop(props, "outline_mask")
            layout.prop(props, "normal_texture")
            layout.prop(props, "face_shadow_mask")
            layout.prop(props, "matcap_texture")
            layout.operator("hitagi.create_toon_preview_nodes")
            layout.operator("hitagi.export_toon_usda")


    class HITAGI_OT_export_toon_usda(Operator):
        bl_idname = "hitagi.export_toon_usda"
        bl_label = "Export Hitagi Toon USDA"
        filepath: StringProperty(name="File Path", subtype="FILE_PATH", default="//hitagi_toon_export.usda")

        def execute(self, context):
            material = context.material
            if material is None:
                self.report({"ERROR"}, "No active material")
                return {"CANCELLED"}
            data = material_to_export_data(material, Path(bpy.path.abspath(self.filepath)))
            mesh = MeshExportData(material_name=data.name)
            path = Path(bpy.path.abspath(self.filepath))
            issues = preflight_toon_export([data], [mesh], path)
            errors = [issue.message for issue in issues if issue.severity == "error"]
            if errors:
                self.report({"ERROR"}, errors[0])
                return {"CANCELLED"}
            write_hitagi_toon_usda(path, [data], [mesh])
            self.report({"INFO"}, f"Wrote {path}")
            return {"FINISHED"}

        def invoke(self, context, event):
            context.window_manager.fileselect_add(self)
            return {"RUNNING_MODAL"}


    class HITAGI_OT_create_toon_preview_nodes(Operator):
        bl_idname = "hitagi.create_toon_preview_nodes"
        bl_label = "Create Toon Preview Nodes"

        def execute(self, context):
            material = context.material
            if material is None:
                self.report({"ERROR"}, "No active material")
                return {"CANCELLED"}
            group = ensure_hitagi_toon_preview_node_group()
            material.use_nodes = True
            tree = material.node_tree
            group_node = tree.nodes.new("ShaderNodeGroup")
            group_node.node_tree = group
            group_node.location = (0, 0)
            output = next((node for node in tree.nodes if node.bl_idname == "ShaderNodeOutputMaterial"), None)
            if output and "BSDF" in group_node.outputs and "Surface" in output.inputs:
                tree.links.new(group_node.outputs["BSDF"], output.inputs["Surface"])
            self.report({"INFO"}, "Created Hitagi Toon preview node group")
            return {"FINISHED"}


    def material_to_export_data(material: Material, usd_path: Path) -> HitagiToonMaterial:
        props = material.hitagi_toon

        def rel(path_value: str) -> str:
            if not path_value:
                return ""
            absolute = Path(bpy.path.abspath(path_value))
            try:
                return absolute.relative_to(usd_path.parent).as_posix()
            except ValueError:
                return absolute.as_posix()

        textures = [
            TextureSlot("baseTexture", "BaseTexture", rel(props.base_texture), "rgb"),
            TextureSlot("shadeRamp", "ShadeRamp", rel(props.shade_ramp), "rgb"),
            TextureSlot("outlineMask", "OutlineMask", rel(props.outline_mask), "r"),
            TextureSlot("normalTexture", "NormalTexture", rel(props.normal_texture), "rgb"),
            TextureSlot("faceShadowMask", "FaceShadowMask", rel(props.face_shadow_mask), "r"),
            TextureSlot("matcapTexture", "MatcapTexture", rel(props.matcap_texture), "rgb"),
        ]
        textures = [texture for texture in textures if texture.path]
        return HitagiToonMaterial(
            name=material.name.replace(" ", "_"),
            base_color=tuple(props.base_color),
            shade_color_1=tuple(props.shade_color_1),
            shade_color_2=tuple(props.shade_color_2),
            shadow_threshold=props.shadow_threshold,
            shadow_softness=props.shadow_softness,
            rim_color=tuple(props.rim_color),
            rim_width=props.rim_width,
            rim_intensity=props.rim_intensity,
            outline_color=tuple(props.outline_color),
            outline_width=props.outline_width,
            specular_threshold=props.specular_threshold,
            specular_intensity=props.specular_intensity,
            emission_color=tuple(props.emission_color),
            textures=textures,
        )


    CLASSES = (HitagiToonProperties, HITAGI_PT_toon_material, HITAGI_OT_export_toon_usda, HITAGI_OT_create_toon_preview_nodes)


    def register():
        for cls in CLASSES:
            bpy.utils.register_class(cls)
        Material.hitagi_toon = PointerProperty(type=HitagiToonProperties)


    def unregister():
        del Material.hitagi_toon
        for cls in reversed(CLASSES):
            bpy.utils.unregister_class(cls)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-sample", type=Path, help="Write a sample HitagiToonSurface USDA")
    args = parser.parse_args(argv)

    if args.write_sample:
        write_hitagi_toon_usda(args.write_sample, [sample_toon_material()], [MeshExportData()])
        print(f"wrote {args.write_sample}")
        return 0
    parser.print_help()
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
