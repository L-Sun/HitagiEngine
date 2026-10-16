"""Portable static USD contract shared by the Blender UI and batch tooling.

Meshes are evaluated, triangulated and split by material. World transforms are
baked into right-handed Y-up metres; normals and UV seams remain face-varying.
Only the documented PreviewSurface + anime inputs are exported, not arbitrary
Blender shader graphs. No dependency on the Hitagi source tree at runtime.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import tempfile
import time
import uuid

import bpy
from mathutils import Matrix, Vector
from pxr import Gf, Sdf, Usd, UsdGeom, UsdShade, Vt

AXIS = Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, -1, 0, 0), (0, 0, 0, 1)))
KINDS = ('GENERIC', 'SKIN', 'FACE', 'HAIR', 'EYE')


def head_frame(scene):
    settings = scene.hitagi
    matrix = settings.head.matrix_world if settings.head else Matrix.Translation(settings.head_center)
    if abs(matrix.to_3x3().determinant()) < 1e-10:
        raise ValueError('Head reference has a singular transform')
    # Blender head axes: +X right, +Y forward, +Z up.
    right = (AXIS.to_3x3() @ matrix.to_3x3().col[0]).normalized()
    up = (AXIS.to_3x3() @ matrix.to_3x3().col[2]).normalized()
    if abs(right.dot(up)) > 1e-4:
        raise ValueError('Head reference must have orthogonal axes (no shear)')
    return AXIS @ matrix.translation, right, up, settings.face_width


def base_material(material):
    settings = material.hitagi
    tint = tuple(settings.base_tint)
    if settings.base_image:
        return tint, settings.base_image
    if not material.use_nodes:
        return tuple(a*b for a, b in zip(material.diffuse_color, tint)), None
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
        return tint, image_node.image
    base = (*tuple(color.default_value[:3]), float(node.inputs['Alpha'].default_value))
    return tuple(a*b for a, b in zip(base, tint)), None


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
                for source in (image, mat.hitagi.control_image):
                    if source and (source.source not in {'FILE', 'GENERATED'} or source.size[0] == 0):
                        errors.append(f'{mat.name}: missing/unsupported image {source.name}')
                if mat.hitagi.control_image and mat.hitagi.control_image.colorspace_settings.name != 'Non-Color':
                    errors.append(f'{mat.name}: control map must use Non-Color data')
                if mat.hitagi.kind in {'FACE', 'HAIR'} and not mat.hitagi.control_image:
                    warnings.append(f'{mat.name}: no control map; viewer uses analytic fallback')
            except ValueError as exc:
                errors.append(str(exc))
        if obj.modifiers or obj.data.shape_keys:
            warnings.append(f'{obj.name}: current evaluated pose is baked; animation is not exported')
    try:
        head_frame(context.scene)
    except ValueError as exc:
        errors.append(str(exc))
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
    center, right, up, width = head_frame(context.scene)
    unit = context.scene.unit_settings.scale_length
    center *= unit
    width *= unit
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
            for name, value in {'animeMaterial': KINDS.index(settings.kind), 'animeSchema': 1,
                                'opacityThreshold': settings.alpha_cutoff, 'roughness': .8}.items():
                surface.CreateInput(name, Sdf.ValueTypeNames.Float).Set(float(value))
            for name, value in {'animeHead': (*center, width), 'animeHeadRight': (*right, 0), 'animeHeadUp': (*up, 0)}.items():
                surface.CreateInput(name, Sdf.ValueTypeNames.Float4).Set(Gf.Vec4f(*value))
            base, image = base_material(material)
            surface.CreateInput('diffuseColor', Sdf.ValueTypeNames.Color3f).Set(Gf.Vec3f(*base[:3]))
            surface.CreateInput('opacity', Sdf.ValueTypeNames.Float).Set(base[3])
            surface.CreateInput('animeBaseTint', Sdf.ValueTypeNames.Float4).Set(Gf.Vec4f(*base))
            for name, source in (('diffuseColor', image), ('animeControl', settings.control_image)):
                if not source:
                    continue
                tex = UsdShade.Shader.Define(stage, path + '/' + name)
                tex.CreateIdAttr('UsdUVTexture')
                tex.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(Sdf.AssetPath(texture_file(source)))
                tex.CreateInput('sourceColorSpace', Sdf.ValueTypeNames.Token).Set('raw' if name == 'animeControl' else 'sRGB')
                tex.CreateInput('wrapS', Sdf.ValueTypeNames.Token).Set('clamp')
                tex.CreateInput('wrapT', Sdf.ValueTypeNames.Token).Set('clamp')
                uv = UsdShade.Shader.Define(stage, path + '/UV')
                uv.CreateIdAttr('UsdPrimvarReader_float2')
                uv.CreateInput('varname', Sdf.ValueTypeNames.Token).Set('st')
                uv.CreateOutput('result', Sdf.ValueTypeNames.Float2)
                tex.CreateInput('st', Sdf.ValueTypeNames.Float2).ConnectToSource(uv.ConnectableAPI(), 'result')
                tex.CreateOutput('rgb', Sdf.ValueTypeNames.Float3)
                surface.CreateInput(name, Sdf.ValueTypeNames.Color3f).ConnectToSource(tex.ConnectableAPI(), 'rgb')

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
                    textured = bool(base_material(material)[1] or material.hitagi.kind == 'HAIR' and material.hitagi.control_image)
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
    """Native USD importer handles geometry; restore the game material metadata."""
    stage = Usd.Stage.Open(str(Path(source).resolve()))
    if not stage:
        raise ValueError('Cannot open USD')
    existing = set(bpy.data.materials)
    bpy.ops.wm.usd_import(filepath=str(Path(source).resolve()), import_cameras=False, import_lights=False,
                          import_materials=True, mtl_name_collision_mode='MAKE_UNIQUE')
    created = [m for m in bpy.data.materials if m not in existing]
    unit = UsdGeom.GetStageMetersPerUnit(stage)
    y_up = UsdGeom.GetStageUpAxis(stage) == UsdGeom.Tokens.y
    restored_head = False
    for prim in stage.Traverse():
        if not prim.IsA(UsdShade.Material):
            continue
        name = prim.GetName()
        candidates = [m for m in created if m.name == name or m.name.startswith(name + '.')]
        if len(candidates) != 1:
            raise ValueError(f'Ambiguous imported material {name}')
        material = candidates[0]
        shader = UsdShade.Material(prim).ComputeSurfaceSource()[0]
        kind = shader.GetInput('animeMaterial').Get() if shader else None
        if kind is not None:
            material.hitagi.kind = KINDS[int(kind)]
        control = shader.GetInput('animeControl') if shader else None
        if control and control.HasConnectedSource():
            tex = UsdShade.Shader(control.GetConnectedSource()[0].GetPrim())
            asset = tex.GetInput('file').Get()
            path = asset.resolvedPath or str(Path(source).parent / asset.path)
            image = bpy.data.images.load(path, check_existing=True)
            image.colorspace_settings.name = 'Non-Color'
            material.hitagi.control_image = image
        if shader:
            base_input = shader.GetInput('diffuseColor')
            tint = shader.GetInput('animeBaseTint').Get()
            if base_input and base_input.HasConnectedSource() and tint is not None:
                material.hitagi.base_tint = tuple(tint)
            head = shader.GetInput('animeHead').Get()
            if head is not None and not restored_head:
                position = Vector(head[:3]) * unit
                position = (AXIS.inverted() @ position if y_up else position) / context.scene.unit_settings.scale_length
                context.scene.hitagi.head = None
                context.scene.hitagi.head_center = position
                context.scene.hitagi.face_width = float(head[3]) * unit / context.scene.unit_settings.scale_length
                right = shader.GetInput('animeHeadRight').Get()
                up = shader.GetInput('animeHeadUp').Get()
                if right is not None and up is not None:
                    axis = AXIS.inverted().to_3x3() if y_up else Matrix.Identity(3)
                    r, u = axis @ Vector(right[:3]), axis @ Vector(up[:3])
                    f = -r.cross(u)
                    reference = bpy.data.objects.new('Hitagi Head', None)
                    context.scene.collection.objects.link(reference)
                    reference.matrix_world = Matrix((r, f, u)).transposed().to_4x4()
                    reference.location = position
                    reference.empty_display_size = context.scene.hitagi.face_width
                    context.scene.hitagi.head = reference
                restored_head = True
            cutoff = shader.GetInput('opacityThreshold').Get()
            if cutoff is not None:
                material.hitagi.alpha_cutoff = cutoff
    context.view_layer.update()
    return created


def send_to_viewer(bridge, asset, timeout=15):
    """Use the viewer's existing mailbox protocol; no socket or new server."""
    bridge = Path(bridge).resolve()
    if not (bridge / 'requests').is_dir():
        raise ValueError('Start usd-viewer-game with this bridge directory first')
    identifier = str(uuid.uuid4())
    request = bridge / 'requests' / f'{identifier}.json'
    temporary = request.with_suffix('.tmp')
    response = bridge / 'responses' / request.name
    temporary.write_text(json.dumps({'tool': 'open', 'path': str(Path(asset).resolve())}), encoding='utf-8')
    temporary.replace(request)
    start = time.monotonic()
    try:
        while not response.exists():
            if time.monotonic() - start > timeout:
                raise TimeoutError('Viewer did not respond; restore its window and check the bridge path')
            time.sleep(.05)
        result = json.loads(response.read_text(encoding='utf-8'))
        if not result.get('ok'):
            raise ValueError(result.get('error', 'Viewer rejected the asset'))
        return result
    finally:
        request.unlink(missing_ok=True)
        temporary.unlink(missing_ok=True)
        response.unlink(missing_ok=True)
