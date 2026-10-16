"""Download the CC0 beta Shino sample and bake a relaxed glTF pose to USD.

Only the static triangle/texture subset needed by this sample is converted.
Requires numpy and Pillow. No Blender or USD Python installation is needed.
"""
import hashlib
import io
import json
from pathlib import Path
import struct
import urllib.request

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "assets/anime_viewer"
URL = "https://raw.githubusercontent.com/madjin/vrm-samples/e16eb187100149a315ad92c3c9968f1d5baa6c7d/vroid/beta/Sendagaya_Shino.vrm"
SHA256 = "1e177c1a7b14f783a9c48395831db8616260d3bddd4154cb2784b779adca49b5"


def prepare():
    DEST.mkdir(parents=True, exist_ok=True)
    source = DEST / "Sendagaya_Shino.vrm"
    if not source.exists():
        urllib.request.urlretrieve(URL, source)
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != SHA256:
        raise ValueError("Sample checksum changed; inspect its provenance before updating the pin")
    magic, version, length = struct.unpack_from("<III", raw)
    assert magic == 0x46546C67 and version == 2 and length == len(raw)
    size = struct.unpack_from("<I", raw, 12)[0]
    doc = json.loads(raw[20:20 + size])
    binary = raw[28 + size:]
    meta = doc["extensions"]["VRM"]["meta"]
    if meta["licenseName"] != "CC0":
        raise ValueError("Unexpected model license")

    def accessor(index):
        a = doc["accessors"][index]
        if "sparse" in a:
            raise ValueError("Sparse accessor is not supported by this sample converter")
        v = doc["bufferViews"][a["bufferView"]]
        dtype = np.dtype({5121: "u1", 5123: "<u2", 5125: "<u4", 5126: "<f4"}[a["componentType"]])
        count = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
        return np.ndarray((a["count"], count), dtype=dtype, buffer=binary,
                          offset=v.get("byteOffset", 0) + a.get("byteOffset", 0),
                          strides=(v.get("byteStride", count * dtype.itemsize), dtype.itemsize)).copy()

    world = {}
    bones = {b["node"]: b["bone"] for b in doc["extensions"]["VRM"]["humanoid"]["humanBones"]}

    def visit(index, parent):
        n = doc["nodes"][index]
        if "matrix" in n:
            local = np.array(n["matrix"]).reshape(4, 4).T
        else:
            x, y, z, w = n.get("rotation", [0, 0, 0, 1])
            local = np.eye(4)
            local[:3, :3] = [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                            [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                            [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]
            local[:3, :3] *= n.get("scale", [1, 1, 1])
            local[:3, 3] = n.get("translation", [0, 0, 0])
        # Keep a reproducible static display pose, with arms lowered 62 degrees.
        if bones.get(index) in {"leftUpperArm", "rightUpperArm"}:
            angle = np.deg2rad(62 if bones[index] == "leftUpperArm" else -62)
            c, s = np.cos(angle), np.sin(angle)
            rotation = np.array([[c,-s,0],[s,c,0],[0,0,1]])
            local[:3,:3] = local[:3,:3] @ rotation
        world[index] = parent @ local
        for child in n.get("children", []):
            visit(child, world[index])

    for index in doc["scenes"][doc.get("scene", 0)]["nodes"]:
        visit(index, np.eye(4))

    def array(values):
        if values.ndim == 1:
            return "[" + ",".join(str(int(v)) for v in values) + "]"
        return "[" + ",".join("(" + ",".join(f"{float(v):.7g}" for v in row) + ")" for row in values) + "]"

    lines = ['#usda 1.0', '(\n defaultPrim = "World"\n upAxis = "Y"\n metersPerUnit = 1\n)', 'def Xform "World" {', 'def Scope "Looks" {']
    for index, material in enumerate(doc["materials"]):
        pbr = material.get("pbrMetallicRoughness", {})
        factor = pbr.get("baseColorFactor", [1, 1, 1, 1])
        lines += [f'def Material "M{index}" {{', f'token outputs:surface.connect = </World/Looks/M{index}/Surface.outputs:surface>',
                  'def Shader "Surface" {', 'uniform token info:id = "UsdPreviewSurface"', 'token outputs:surface',
                  'float inputs:roughness = 0.8',
                  f'float inputs:opacityThreshold = {0.0 if material.get("alphaMode", "OPAQUE") == "OPAQUE" else 0.3}']
        if "baseColorTexture" in pbr:
            if material.get("alphaMode", "OPAQUE") != "OPAQUE":
                lines += [f'float inputs:opacity.connect = </World/Looks/M{index}/Texture.outputs:a>']
            texture = doc["textures"][pbr["baseColorTexture"]["index"]]
            img = doc["images"][texture["source"]]
            view = doc["bufferViews"][img["bufferView"]]
            offset = view.get("byteOffset", 0)
            image = Image.open(io.BytesIO(binary[offset:offset+view["byteLength"]])).convert("RGBA")
            pixels = np.asarray(image).astype(np.float32) * np.array(factor)
            Image.fromarray(np.clip(pixels, 0, 255).astype(np.uint8)).save(DEST / f"material_{index}.png")
            lines += [f'color3f inputs:diffuseColor.connect = </World/Looks/M{index}/Texture.outputs:rgb>', '}',
                      'def Shader "Texture" {', 'uniform token info:id = "UsdUVTexture"',
                      f'asset inputs:file = @material_{index}.png@',
                      f'float2 inputs:st.connect = </World/Looks/M{index}/UV.outputs:result>',
                      'float3 outputs:rgb', 'float outputs:a', '}', 'def Shader "UV" {',
                      'uniform token info:id = "UsdPrimvarReader_float2"', 'string inputs:varname = "st"',
                      'float2 outputs:result', '}']
        else:
            lines += [f'color3f inputs:diffuseColor = ({factor[0]}, {factor[1]}, {factor[2]})', '}']
        lines += ['}']
    lines += ['}']
    triangles = 0
    # Bake the display pose; the USD viewer itself is static, not an animator.
    for ni, node in enumerate(doc["nodes"]):
        if "mesh" not in node or ni not in world:
            continue
        for pi, primitive in enumerate(doc["meshes"][node["mesh"]]["primitives"]):
            if primitive.get("mode", 4) != 4:
                raise ValueError("Only triangles are supported")
            attrs = primitive["attributes"]
            pos = accessor(attrs["POSITION"])
            normals = accessor(attrs["NORMAL"])
            transforms = np.broadcast_to(world[ni], (len(pos), 4, 4))
            if "skin" in node:
                skin = doc["skins"][node["skin"]]
                inverse = accessor(skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
                joints = np.array([world[j] for j in skin["joints"]]) @ inverse
                ids = accessor(attrs["JOINTS_0"])
                weights = accessor(attrs["WEIGHTS_0"])
                transforms = (joints[ids] * weights[:, :, None, None]).sum(axis=1)
            pos = np.einsum("nij,nj->ni", transforms, np.column_stack((pos, np.ones(len(pos)))))[:, :3]
            normals = np.einsum("nij,nj->ni", np.linalg.inv(transforms[:, :3, :3]).transpose(0, 2, 1), normals)
            normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-8)
            indices = accessor(primitive["indices"]).reshape(-1)
            triangles += len(indices)//3
            uv = accessor(attrs["TEXCOORD_0"])
            # glTF uses top-left UVs; USD and Hitagi's PNG decoder use bottom-left.
            uv[:, 1] = 1 - uv[:, 1]
            lines += [f'def Mesh "Mesh_{ni}_{pi}" (prepend apiSchemas = ["MaterialBindingAPI"]) {{', 'uniform token subdivisionScheme = "none"',
                      'bool doubleSided = true', f'point3f[] points = {array(pos)}',
                      f'normal3f[] normals = {array(normals)} (interpolation = "vertex")',
                      f'texCoord2f[] primvars:st = {array(uv)} (interpolation = "vertex")',
                      f'int[] faceVertexCounts = {array(np.full(len(indices)//3, 3))}',
                      f'int[] faceVertexIndices = {array(indices)}',
                      f'rel material:binding = </World/Looks/M{primitive.get("material", 0)}>', '}']
    lines += ['}']
    (DEST / "shino.usda").write_text("\n".join(lines), encoding="utf-8")
    (DEST / "provenance.json").write_text(json.dumps({"url": URL, "sha256": hashlib.sha256(raw).hexdigest(), "metadata": meta, "triangles": triangles}, indent=2), encoding="utf-8")
    print(f"Prepared {triangles} triangles: {DEST / 'shino.usda'}")


if __name__ == "__main__":
    prepare()
