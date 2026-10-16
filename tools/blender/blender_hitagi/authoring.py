"""Non-destructive character preparation. Generated maps are editable seeds,
not recovered game assets or an automatic substitute for art direction."""
import math

import bpy
import numpy as np
from mathutils import Matrix, Vector

from .pipeline import AXIS, base_material, head_frame, selected_meshes


def new_image(name, pixels):
    height, width = pixels.shape[:2]
    image = bpy.data.images.new(name, width=width, height=height, alpha=True, float_buffer=False)
    image.colorspace_settings.name = 'Non-Color'
    image.pixels.foreach_set(np.asarray(pixels, dtype=np.float32).ravel())
    image.update()
    image.pack()
    return image


def face_map(size, cheek, nose):
    # Rows are bottom-up, as Blender image pixels and USD UVs are.
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    x = (x + .5) / size * 2 - 1
    y = (y + .5) / size * 2 - 1
    curve = cheek * np.exp(-((y + .30) / .38)**2)
    nose_shape = nose * np.exp(-(x / .16)**2 - ((y + .08) / .25)**2)
    r = np.clip(.52 + .43 * x + curve - nose_shape, 0, 1)
    g = np.clip(.52 - .43 * x + curve - nose_shape, 0, 1)
    forehead = np.clip((y + .22) / .42, 0, 1)
    forehead = forehead * forehead * (3 - 2 * forehead)
    return np.stack((r, g, forehead, np.ones_like(x)), axis=-1)


def smooth_skin_normals(obj, center, width, kind, strength):
    mesh = obj.data
    mesh.calc_loop_triangles()
    original = np.array([n.vector[:] for n in mesh.corner_normals], dtype=np.float64)
    positions = np.array([v.co[:] for v in mesh.vertices])
    if kind == 'FACE':
        # An ellipsoid normal field removes accidental small triangles while
        # retaining part of the source normals around the mouth and nose.
        local_center = np.array(center)
        radii = np.array((width, width * .85, width))
        desired = (positions - local_center) / (radii * radii)
        desired /= np.maximum(np.linalg.norm(desired, axis=1, keepdims=True), 1e-8)
    else:
        desired = np.array([v.normal[:] for v in mesh.vertices], dtype=np.float64)
        adjacency = [set() for _ in mesh.vertices]
        for edge in mesh.edges:
            a, b = edge.vertices
            adjacency[a].add(b)
            adjacency[b].add(a)
        for _ in range(2):
            previous = desired.copy()
            for i, neighbors in enumerate(adjacency):
                compatible = [j for j in neighbors if np.dot(previous[i], previous[j]) > .7]
                if compatible:
                    desired[i] = .5 * previous[i] + .5 * previous[compatible].mean(axis=0)
            desired /= np.maximum(np.linalg.norm(desired, axis=1, keepdims=True), 1e-8)
    normals = []
    for i, loop in enumerate(mesh.loops):
        value = original[i] * (1 - strength) + desired[loop.vertex_index] * strength
        value /= max(np.linalg.norm(value), 1e-8)
        normals.append(value.tolist())
    mesh.normals_split_custom_set(normals)
    mesh.update()


def hair_map(context, obj, center, width, size):
    mesh = obj.data
    # Untextured hair can acquire an atlas without damaging painted base UVs.
    # Textured hair keeps its authored UV; overlapping islands need manual care.
    if not any(base_material(m)[1] for m in mesh.materials):
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        context.view_layer.objects.active = obj
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=.015)
        bpy.ops.object.mode_set(mode='OBJECT')
    mesh.calc_loop_triangles()
    pixels = np.zeros((size, size, 4), dtype=np.float32)
    pixels[..., :3] = (.5, 1, .65)
    occupied = np.zeros((size, size), dtype=bool)
    positions = np.array([v.co[:] for v in mesh.vertices])
    # Author a root-to-tip seed: fringe radiates from a point above the head;
    # long locks gradually align with the up axis. Sign is irrelevant to the
    # symmetric anisotropic lobe. The result can subsequently be painted.
    root = np.asarray(center) + np.array((0, 0, width * .7))
    direction = root - positions
    weight = np.clip((positions[:, 2] - center.z + width) / (2 * width), 0, 1)
    direction /= np.maximum(np.linalg.norm(direction, axis=1, keepdims=True), 1e-8)
    direction = direction * weight[:, None] + np.array((0, 0, 1)) * (1 - weight[:, None])
    for tri in mesh.loop_triangles:
        texture_uv = np.array([mesh.uv_layers.active.data[i].uv[:] for i in tri.loops])
        uv = texture_uv * size - .5
        lo = np.maximum(np.floor(uv.min(axis=0)).astype(int), 0)
        hi = np.minimum(np.ceil(uv.max(axis=0)).astype(int), size - 1)
        if np.any(hi < lo):
            continue
        a, b, c = uv
        denominator = (b[1]-c[1])*(a[0]-c[0]) + (c[0]-b[0])*(a[1]-c[1])
        if abs(denominator) < 1e-8:
            continue
        # Encode in the UV tangent frame. This remains valid when the artist
        # later rotates the mesh or when export bakes its world transform.
        p0, p1, p2 = positions[list(tri.vertices)]
        duv1, duv2 = texture_uv[1]-texture_uv[0], texture_uv[2]-texture_uv[0]
        determinant = duv1[0]*duv2[1]-duv1[1]*duv2[0]
        tangent = ((p1-p0)*duv2[1]-(p2-p0)*duv1[1]) / determinant
        bitangent = ((p2-p0)*duv1[0]-(p1-p0)*duv2[0]) / determinant
        encoded = []
        for vertex, loop in zip(tri.vertices, tri.loops):
            n = np.array(mesh.corner_normals[loop].vector)
            t = tangent - n * np.dot(n, tangent)
            t /= max(np.linalg.norm(t), 1e-8)
            b = np.cross(n, t) * (1 if np.dot(np.cross(n, t), bitangent) >= 0 else -1)
            flow = np.array((np.dot(direction[vertex], t), np.dot(direction[vertex], b), 0))
            flow /= max(np.linalg.norm(flow), 1e-8)
            encoded.append(flow)
        yy, xx = np.mgrid[lo[1]:hi[1]+1, lo[0]:hi[0]+1]
        u = ((b[1]-c[1])*(xx-c[0]) + (c[0]-b[0])*(yy-c[1])) / denominator
        v = ((c[1]-a[1])*(xx-c[0]) + (a[0]-c[0])*(yy-c[1])) / denominator
        w = 1-u-v
        inside = (u >= -1e-5) & (v >= -1e-5) & (w >= -1e-5)
        values = u[..., None]*encoded[0] + v[..., None]*encoded[1] + w[..., None]*encoded[2]
        values /= np.maximum(np.linalg.norm(values, axis=-1, keepdims=True), 1e-8)
        region = pixels[lo[1]:hi[1]+1, lo[0]:hi[0]+1]
        region[inside, :3] = values[inside] * .5 + .5
        region[inside, 2] = .65
        region[inside, 3] = .85
        occupied[lo[1]:hi[1]+1, lo[0]:hi[0]+1] |= inside
    if not occupied.any():
        raise ValueError(f'{obj.name}: cannot bake a direction map into degenerate UVs')
    # Four-pixel dilation prevents black seams under bilinear sampling.
    for _ in range(4):
        previous = occupied.copy()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            src = np.roll(previous, (dy, dx), (0, 1))
            if dy == 1: src[0] = False
            if dy == -1: src[-1] = False
            if dx == 1: src[:, 0] = False
            if dx == -1: src[:, -1] = False
            fill = src & ~occupied
            pixels[fill] = np.roll(pixels, (dy, dx), (0, 1))[fill]
            occupied |= fill
    return pixels


def prepare_copy(context):
    originals = selected_meshes(context)
    if not originals or context.mode != 'OBJECT':
        raise ValueError('Select meshes in Object Mode')
    settings = context.scene.hitagi
    center, _, _, width = head_frame(context.scene)
    center = AXIS.inverted() @ center
    if settings.head and settings.head.matrix_world.to_quaternion().angle > 1e-5:
        raise ValueError('Automatic map preparation expects an upright head; custom oriented maps can still be exported')
    collection = bpy.data.collections.new('Hitagi Prepared')
    context.scene.collection.children.link(collection)
    copies = []
    try:
        for original in originals:
            if original.matrix_world.determinant() <= 0:
                raise ValueError(f'{original.name}: automatic preparation requires a positive world scale; export supports mirrored objects')
            if any(m and m.hitagi.kind in {'FACE', 'HAIR', 'SKIN'} for m in original.data.materials) and len(original.data.materials) != 1:
                raise ValueError(f'{original.name}: separate Face/Hair/Skin by material before preparation; export itself supports multiple slots')
            if original.modifiers or original.data.shape_keys:
                raise ValueError(f'{original.name}: prepare a static mesh copy with modifiers applied first')
            obj = original.copy()
            obj.data = original.data.copy()
            collection.objects.link(obj)
            obj.parent = None
            obj.matrix_world = original.matrix_world.copy()
            # Preserve transformed custom normals explicitly when baking world space.
            normal_matrix = obj.matrix_world.to_3x3().inverted().transposed()
            normals = [(normal_matrix @ n.vector).normalized() for n in obj.data.corner_normals]
            obj.data.transform(obj.matrix_world)
            obj.matrix_world = Matrix.Identity(4)
            obj.data.normals_split_custom_set(normals)
            for i, mat in enumerate(obj.data.materials):
                if mat is None:
                    raise ValueError(f'{obj.name}: missing material')
                obj.data.materials[i] = mat.copy()
            copies.append(obj)
            kind = obj.data.materials[0].hitagi.kind if len(obj.data.materials) == 1 else 'GENERIC'
            if kind in {'FACE', 'SKIN'}:
                smooth_skin_normals(obj, center, width, kind, settings.normal_strength)
            if kind == 'FACE':
                image = new_image(obj.name + '_FaceControl', face_map(settings.map_size, settings.cheek, settings.nose))
                obj.data.materials[0].hitagi.control_image = image
            elif kind == 'HAIR':
                image = new_image(obj.name + '_HairFlow', hair_map(context, obj, center, width, min(settings.map_size * 2, 2048)))
                obj.data.materials[0].hitagi.control_image = image
            obj['hitagi_prepared_from'] = original.name
        bpy.ops.object.select_all(action='DESELECT')
        for obj in copies:
            obj.select_set(True)
        context.view_layer.objects.active = copies[0]
        return copies
    except Exception:
        if context.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        for obj in list(collection.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.collections.remove(collection)
        for obj in originals:
            obj.select_set(True)
        context.view_layer.objects.active = originals[0]
        raise
