"""Portable static USD contract shared by the Blender UI and batch tooling.

Meshes are evaluated, triangulated and split by material. World transforms are
baked into right-handed Y-up metres; normals and UV seams remain face-varying.
Only the documented PreviewSurface base color and opacity are exported, not arbitrary
Blender shader graphs. No dependency on the Hitagi source tree at runtime.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import tempfile

import bpy
from mathutils import Matrix
from pxr import Gf, Sdf, Usd, UsdGeom, UsdShade

AXIS = Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, -1, 0, 0), (0, 0, 0, 1)))


def base_material(material):
    settings = material.hitagi
    if settings.base_image:
        return (1, 1, 1, 1), settings.base_image
    if not material.use_nodes:
        return tuple(material.diffuse_color), None
    output = next((n for n in material.node_tree.nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output), None)
    if not output or not output.inputs['Surface'].is_linked:
        raise ValueError(f'{material.name}: no active material surface')
    node = output.inputs['Surface'].links[0].from_node
    if node.type != 'BSDF_PRINCIPLED':
        raise ValueError(f'{material.name}: use Principled BSDF or assign an explicit Base Image')
    color = node.inputs['Base Color']
    if color.is_linked:
        link = color.links[0]
        if link.from_node.type != 'TEX_IMAGE' or not link.from_node.image or link.from_socket.name != 'Color':
            raise ValueError(f'{material.name}: bake the Base Color graph to one image before export')
        image_node = link.from_node
        if image_node.inputs['Vector'].is_linked:
            uv_node = image_node.inputs['Vector'].links[0].from_node
            if uv_node.type != 'UVMAP':
                raise ValueError(f'{material.name}: mapped/generated texture coordinates must be baked')
        return (1, 1, 1, 1), image_node.image
    base = (*tuple(color.default_value[:3]), float(node.inputs['Alpha'].default_value))
    return base, None


def selected_meshes(context):
    return sorted((o for o in context.selected_objects if o.type == 'MESH'), key=lambda o: o.name)


def validate(context, objects):
    errors, warnings = [], []
    if not objects:
        errors.append('Select at least one mesh')
    if context.mode != 'OBJECT':
        errors.append('Switch to Object Mode before export')
    for obj in objects:
        if abs(obj.matrix_world.determinant()) < 1e-10:
            errors.append(f'{obj.name}: singular transform')
        if not obj.data.uv_layers:
            errors.append(f'{obj.name}: missing primary UV map')
        if any(not all(math.isfinite(v) for v in vertex.co) for vertex in obj.data.vertices):
            errors.append(f'{obj.name}: non-finite position')
        if not obj.data.materials or any(m is None for m in obj.data.materials):
            errors.append(f'{obj.name}: every material slot must be assigned')
        for mat in obj.data.materials:
            if mat is None:
                continue
            try:
                _, image = base_material(mat)
                if image and mat.use_nodes and not mat.hitagi.base_image and obj.data.uv_layers.active:
                    for node in mat.node_tree.nodes:
                        if node.type == 'UVMAP' and node.uv_map and node.uv_map != obj.data.uv_layers.active.name:
                            errors.append(f'{mat.name}: Base Color must use the active UV map')
                if image and (image.source not in {'FILE', 'GENERATED'} or image.size[0] == 0):
                    errors.append(f'{mat.name}: missing/unsupported image {image.name}')
            except ValueError as exc:
                errors.append(str(exc))
        if obj.modifiers or obj.data.shape_keys:
            warnings.append(f'{obj.name}: current evaluated pose is baked; animation is not exported')
    return {'errors': sorted(set(errors)), 'warnings': sorted(set(warnings))}


def export_asset(context, destination, objects=None):
    destination = Path(destination).resolve()
    if destination.suffix.lower() != '.usda':
        raise ValueError('Choose a .usda asset filename')
    objects = selected_meshes(context) if objects is None else objects
    report = validate(context, objects)
    if report['errors']:
        raise ValueError('\n'.join(report['errors']))
    destination.parent.mkdir(parents=True, exist_ok=True)
    unit = context.scene.unit_settings.scale_length
    # All dependencies are immutable content-addressed files. Publish the USD
    # only after staging/validation succeeds; failed exports leave the old USD.
    with tempfile.TemporaryDirectory(prefix='.hitagi-', dir=destination.parent) as temporary:
        work = Path(temporary)
        stage = Usd.Stage.CreateNew(str(work / destination.name))
        root = UsdGeom.Xform.Define(stage, '/World')
        stage.SetDefaultPrim(root.GetPrim())
        UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)
        UsdGeom.SetStageMetersPerUnit(stage, 1.0)
        stage.SetMetadata('customLayerData', {'hitagi_schema': 1, 'generator': 'blender_hitagi/0.1.0'})
        materials = sorted({m for obj in objects for m in obj.data.materials}, key=lambda m: m.name)
        material_paths, texture_cache, dependencies = {}, {}, {}

        def texture_file(image):
            key = image.as_pointer()
            if key in texture_cache:
                return texture_cache[key]
            image_path = Path(bpy.path.abspath(image.filepath)) if image.filepath else None
            target = work / 'image.png'
            if image_path and image_path.is_file() and image_path.suffix.lower() == '.png' and not image.is_dirty and not image.packed_file:
                shutil.copyfile(image_path, target)
            else:
                copy = image.copy()
                try:
                    copy.filepath_raw = str(target)
                    copy.file_format = 'PNG'
                    copy.save()
                finally:
                    bpy.data.images.remove(copy)
            digest = hashlib.sha256(target.read_bytes()).hexdigest()
            relative = f'textures/{digest}.png'
            (work / 'textures').mkdir(exist_ok=True)
            target.replace(work / relative)
            dependencies[relative] = digest
            texture_cache[key] = relative
            return relative

        for index, material in enumerate(materials):
            path = f'/World/Looks/M{index}'
            material_paths[material] = path
            mat = UsdShade.Material.Define(stage, path)
            mat.GetPrim().SetDisplayName(material.name)
            surface = UsdShade.Shader.Define(stage, path + '/Surface')
            surface.CreateIdAttr('UsdPreviewSurface')
            mat.CreateSurfaceOutput().ConnectToSource(surface.ConnectableAPI(), 'surface')
            surface.CreateOutput('surface', Sdf.ValueTypeNames.Token)
            settings = material.hitagi
            surface.CreateInput('opacityThreshold', Sdf.ValueTypeNames.Float).Set(settings.alpha_cutoff)
            surface.CreateInput('roughness', Sdf.ValueTypeNames.Float).Set(.8)
            base, image = base_material(material)
            surface.CreateInput('diffuseColor', Sdf.ValueTypeNames.Color3f).Set(Gf.Vec3f(*base[:3]))
            surface.CreateInput('opacity', Sdf.ValueTypeNames.Float).Set(base[3])
            if image:
                tex = UsdShade.Shader.Define(stage, path + '/BaseColor')
                tex.CreateIdAttr('UsdUVTexture')
                tex.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(Sdf.AssetPath(texture_file(image)))
                tex.CreateInput('sourceColorSpace', Sdf.ValueTypeNames.Token).Set('sRGB')
                tex.CreateInput('wrapS', Sdf.ValueTypeNames.Token).Set('clamp')
                tex.CreateInput('wrapT', Sdf.ValueTypeNames.Token).Set('clamp')
                uv = UsdShade.Shader.Define(stage, path + '/UV')
                uv.CreateIdAttr('UsdPrimvarReader_float2')
                uv.CreateInput('varname', Sdf.ValueTypeNames.Token).Set('st')
                uv.CreateOutput('result', Sdf.ValueTypeNames.Float2)
                tex.CreateInput('st', Sdf.ValueTypeNames.Float2).ConnectToSource(uv.ConnectableAPI(), 'result')
                tex.CreateOutput('rgb', Sdf.ValueTypeNames.Float3)
                surface.CreateInput('diffuseColor', Sdf.ValueTypeNames.Color3f).ConnectToSource(tex.ConnectableAPI(), 'rgb')

        mesh_report = []
        graph = context.evaluated_depsgraph_get()
        for obj_index, obj in enumerate(objects):
            evaluated = obj.evaluated_get(graph)
            mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=graph)
            try:
                mesh.calc_loop_triangles()
                if not mesh.uv_layers.active:
                    raise ValueError(f'{obj.name}: evaluated mesh has no UV map')
                transform = Matrix.Scale(unit, 4) @ AXIS @ evaluated.matrix_world
                normal_matrix = transform.to_3x3().inverted().transposed()
                points = [Gf.Vec3f(*(transform @ v.co)) for v in mesh.vertices]
                uv_data = mesh.uv_layers.active.data
                for slot, material in enumerate(mesh.materials):
                    triangles = [tri for tri in mesh.loop_triangles if tri.material_index == slot]
                    if not triangles:
                        continue
                    indices, normals, uvs = [], [], []
                    textured = bool(base_material(material)[1])
                    for tri in triangles:
                        loops = list(tri.loops)
                        if transform.determinant() < 0:
                            loops.reverse()
                        for loop in loops:
                            indices.append(mesh.loops[loop].vertex_index)
                            normal = (normal_matrix @ mesh.corner_normals[loop].vector).normalized()
                            uv = uv_data[loop].uv
                            if not all(math.isfinite(x) for x in (*normal, *uv)) or normal.length < .99:
                                raise ValueError(f'{obj.name}: invalid evaluated normal or UV')
                            if textured and any(x < -1e-5 or x > 1.00001 for x in uv):
                                raise ValueError(f'{obj.name}: textured UV outside [0,1]; bake repeating/UDIM textures first')
                            normals.append(Gf.Vec3f(*normal))
                            uvs.append(Gf.Vec2f(*uv))
                    usd = UsdGeom.Mesh.Define(stage, f'/World/Mesh_{obj_index}_{slot}')
                    usd.GetPrim().SetDisplayName(obj.name)
                    usd.CreatePointsAttr(points)
                    usd.CreateFaceVertexCountsAttr([3] * len(triangles))
                    usd.CreateFaceVertexIndicesAttr(indices)
                    usd.CreateNormalsAttr(normals)
                    usd.SetNormalsInterpolation(UsdGeom.Tokens.faceVarying)
                    usd.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
                    usd.CreateDoubleSidedAttr(True)
                    UsdGeom.PrimvarsAPI(usd).CreatePrimvar('st', Sdf.ValueTypeNames.TexCoord2fArray, UsdGeom.Tokens.faceVarying).Set(uvs)
                    UsdShade.MaterialBindingAPI.Apply(usd.GetPrim()).Bind(UsdShade.Material(stage.GetPrimAtPath(material_paths[material.original])))
                    mesh_report.append({'object': obj.name, 'material': material.name, 'triangles': len(triangles)})
            finally:
                evaluated.to_mesh_clear()
        if not mesh_report:
            raise ValueError('Selected meshes contain no triangles')
        stage.GetRootLayer().Save()
        stage = None
        asset_hash = hashlib.sha256((work / destination.name).read_bytes()).hexdigest()
        manifest = {'schema': 1, 'generator': 'blender_hitagi/0.1.0', 'blender': bpy.app.version_string,
                    'source_blend': bpy.data.filepath, 'asset': destination.name, 'sha256': asset_hash,
                    'coordinates': 'right-handed Y-up metres, evaluated world transforms baked',
                    'meshes': mesh_report, 'triangles': sum(m['triangles'] for m in mesh_report),
                    'textures': dependencies, **report}
        (work / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
        for relative in dependencies:
            final = destination.parent / relative
            final.parent.mkdir(exist_ok=True)
            if final.exists():
                if hashlib.sha256(final.read_bytes()).hexdigest() != dependencies[relative]:
                    raise ValueError(f'Existing content-addressed texture is corrupt: {final}')
            else:
                os.replace(work / relative, final)
        os.replace(work / destination.name, destination)
        os.replace(work / 'manifest.json', destination.with_suffix('.hitagi.json'))
    return manifest


def import_asset(context, source):
    """Use Blender's native USD importer for geometry and PreviewSurface materials."""
    source = Path(source).resolve()
    existing = set(bpy.data.materials)
    result = bpy.ops.wm.usd_import(filepath=str(source), import_cameras=False, import_lights=False,
                                   import_materials=True, mtl_name_collision_mode='MAKE_UNIQUE')
    if result != {'FINISHED'}:
        raise ValueError(f'Cannot import USD: {source}')
    context.view_layer.update()
    return [m for m in bpy.data.materials if m not in existing]
