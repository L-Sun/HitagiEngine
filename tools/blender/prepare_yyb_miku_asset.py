"""Prepare YYB's default Miku using the Blender–Hitagi exporter.

Run with Blender --background --factory-startup --python-exit-code 1 --python
tools/blender/prepare_yyb_miku_asset.py. Downloads are pinned and local assets
remain git-ignored. Model terms are separate from the engine/plugin license.
"""
import hashlib
import http.cookiejar
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys
import urllib.parse
import urllib.request
import zipfile

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / 'assets/anime_viewer/yyb_miku'
MODEL_HASH = 'fe9c2054e6b333a5a6651913cc0f3bcdfc0da143e165a6ec0b0784dfecd3b607'
TOOLS_HASH = '5b982ad123834f56805ac14a3b1f369c246d8ccba752658146b047de820f24bb'
TOOLS_URL = 'https://github.com/MMD-Blender/blender_mmd_tools/archive/refs/tags/v4.5.14.zip'
MODEL_PAGE = 'https://bowlroll.net/file/284016'
AUTHOR_PAGE = 'https://www.bilibili.com/video/BV1Bg411D7Me/'


def extract(archive, destination, digest):
    if hashlib.sha256(archive.read_bytes()).hexdigest() != digest:
        raise ValueError(f'Unexpected archive revision: {archive}')
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            target = (destination / entry.filename).resolve()
            if not target.is_relative_to(destination.resolve()):
                raise ValueError('Archive contains an unexpected path')
        source.extractall(destination)


def download():
    DEST.mkdir(parents=True, exist_ok=True)
    archive = DEST / 'source.zip'
    if not archive.exists():
        print('Downloading from the original YYB distribution page...', flush=True)
        opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
        page = opener.open(MODEL_PAGE, timeout=60).read().decode('utf-8')
        token = re.search(r'data-csrf_token="([^"]+)"', page).group(1)
        # Public download key posted by the author in the release video's pinned comment.
        body = urllib.parse.urlencode({'csrf_token': token, 'download_key': 'MIKU15TH20220831'}).encode()
        response = json.loads(opener.open('https://bowlroll.net/api/file/284016/download-check', data=body, timeout=60).read())
        if response.get('status') != 'success':
            raise RuntimeError(f'Download unavailable; use the author page: {AUTHOR_PAGE}')
        temporary = archive.with_suffix('.tmp')
        with opener.open(response['url'], timeout=120) as source, temporary.open('wb') as target:
            shutil.copyfileobj(source, target)
        temporary.replace(archive)
    extract(archive, DEST / 'source', MODEL_HASH)
    tools_zip = ROOT / '.local/mmd-tools-v4.5.14.zip'
    if not tools_zip.exists():
        tools_zip.parent.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(TOOLS_URL, tools_zip)
    extract(tools_zip, ROOT / '.local', TOOLS_HASH)
    return ROOT / '.local/blender_mmd_tools-4.5.14/mmd_tools/core/pmx/__init__.py'


def main():
    parser_path = download()
    spec = importlib.util.spec_from_file_location('hitagi_pmx_reader', parser_path)
    pmx = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(pmx)
    model = pmx.load(str(next((DEST / 'source').glob('*.pmx'))))
    sys.path.insert(0, str(Path(__file__).parent))
    import blender_hitagi
    from blender_hitagi import authoring, pipeline
    blender_hitagi.register()
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    # PMX reader reverses source winding. Reflect X while mapping MMD Y-up to
    # Blender Z-up; the resulting outward winding matches the source normals.
    positions = np.array([v.co for v in model.vertices], dtype=np.float64)
    positions = positions[:, [0, 2, 1]] * np.array((-.08, -.08, .08))
    normals = np.array([v.normal for v in model.vertices], dtype=np.float64)[:, [0, 2, 1]] * (-1, -1, 1)
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-8)
    uvs = np.array([v.uv for v in model.vertices], dtype=np.float64)
    uvs[:, 1] = 1 - uvs[:, 1]
    faces = np.asarray(model.faces, dtype=np.int32)
    settings = bpy.context.scene.hitagi
    settings.head_center = (0, .04, 18.15 * .08)
    settings.face_width = 1.05 * .08
    settings.bridge = str(ROOT / 'temp/usd-viewer/live')
    settings.export_path = str(DEST / 'yyb_miku.usda')
    face_control = authoring.new_image('YYB_FaceControl', authoring.face_map(512, .06, .025))
    # These slots are explicit to the pinned YYB asset, not general name guessing.
    omitted = {7: 'MMD eye reflection overlay', 11: 'nearly invisible face overlay',
               13: 'disabled facial morph layer', 16: 'half-transparent nose overlay',
               20: 'duplicate hair AO shell', 21: 'duplicate hair AO shell', 22: 'duplicate hair AO shell',
               23: 'collapsed optional glasses', 24: 'collapsed optional glasses', 25: 'collapsed optional glasses',
               26: 'MMD fake hair shadow (replaced by real hair shadow map)',
               34: 'duplicate skirt AO shell', 36: 'duplicate skirt AO shell', 38: 'duplicate skirt AO shell',
               47: 'duplicate clothes AO shell'}
    skin = {0, 1, 2, 4}
    face = {9, 10}
    eye = {5, 6, 8, 14, 15}
    hair = {17, 18, 19}
    output, groups, skipped = [], [], []
    offset = 0
    for index, material in enumerate(model.materials):
        triangles = faces[offset:offset + material.vertex_count // 3]
        offset += len(triangles)
        if index in omitted:
            skipped.append({'slot': index, 'material': material.name, 'triangles': len(triangles), 'reason': omitted[index]})
            continue
        selected, indices = np.unique(triangles.reshape(-1), return_inverse=True)
        mesh = bpy.data.meshes.new(material.name)
        mesh.from_pydata(positions[selected].tolist(), [], indices.reshape(-1, 3).tolist())
        mesh.update()
        for polygon in mesh.polygons:
            polygon.use_smooth = True
        mesh.normals_split_custom_set_from_vertices(normals[selected].tolist())
        uv = mesh.uv_layers.new(name='UVMap')
        for loop in mesh.loops:
            uv.data[loop.index].uv = uvs[selected[loop.vertex_index]]
        obj = bpy.data.objects.new(material.name, mesh)
        bpy.context.collection.objects.link(obj)
        mat = bpy.data.materials.new(material.name)
        mat.use_nodes = True
        mat.hitagi.kind = 'FACE' if index in face else 'EYE' if index in eye else 'HAIR' if index in hair else 'SKIN' if index in skin else 'GENERIC'
        mat.hitagi.alpha_cutoff = .45
        if index in face or index in skin:
            # Keep the light skin texture below the viewer's display-referred
            # skin ramp clipping point; source image pixels remain unchanged.
            mat.hitagi.base_tint = (.88, .87, .86, 1)
        elif index in hair:
            mat.hitagi.base_tint = (.92, .92, .92, 1)
        if index in face:
            mat.hitagi.control_image = face_control
        bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        bsdf.inputs['Base Color'].default_value = material.diffuse
        if material.texture >= 0:
            source = Path(model.textures[material.texture].path)
            image = bpy.data.images.load(str(source), check_existing=True)
            image.pack()
            texture = mat.node_tree.nodes.new('ShaderNodeTexImage')
            texture.image = image
            texture.extension = 'EXTEND'
            mat.node_tree.links.new(texture.outputs['Color'], bsdf.inputs['Base Color'])
            mat.node_tree.links.new(texture.outputs['Alpha'], bsdf.inputs['Alpha'])
        mesh.materials.append(mat)
        output.append(obj)
        groups.append({'source_slot': index, 'name': material.name, 'kind': mat.hitagi.kind, 'triangles': len(triangles)})
    assert offset == len(faces)
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    for obj in output:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = output[0]
    for readme in (DEST / 'source').glob('Readme-*.png'):
        shutil.copyfile(readme, DEST / readme.name)
    bpy.ops.wm.save_as_mainfile(filepath=str(DEST / 'yyb_miku.blend'))
    manifest = pipeline.export_asset(bpy.context, DEST / 'yyb_miku.usda', output)
    provenance = {'model': model.name, 'author': '三目YYB / SANMUYYB', 'character': 'Hatsune Miku / Crypton Future Media',
        'distribution': MODEL_PAGE, 'author_release': AUTHOR_PAGE, 'sha256': MODEL_HASH,
        'terms': 'Original Readme-EN/CN/JP.png; noncommercial local use, not an MIT/CC0 asset. Do not redistribute original model data.',
        'parser': {'repository': 'https://github.com/MMD-Blender/blender_mmd_tools', 'version': 'v4.5.14', 'sha256': TOOLS_HASH},
        'source_triangles': len(faces), 'render_triangles': manifest['triangles'], 'materials': groups, 'omitted_layers': skipped,
        'conversion': 'Static bind pose. Source geometry/normals/UV retained for exported layers. No MMD sphere maps, physics, animation or custom FX. Face threshold map added; source painted hair textures retained.'}
    (DEST / 'provenance.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'triangles': manifest['triangles'], 'materials': len(groups), 'textures': len(manifest['textures']), 'usd': str(DEST / 'yyb_miku.usda')}, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()
