"""Run with Blender --background --python this_file.py -- [--output DIR].

Uses the installed/exportable plugin API rather than a second asset exporter.
The original USD and its author notices are preserved; only the new .blend and
portable asset package are written to the destination.
"""
import argparse
import json
from pathlib import Path
import shutil
import sys

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
import blender_hitagi
from blender_hitagi import pipeline, authoring


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / 'assets/anime_viewer/miku_hitagi')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    blender_hitagi.register()
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    source = ROOT / 'assets/anime_viewer/miku'
    pipeline.import_asset(bpy.context, source / 'miku.usda')
    originals = pipeline.selected_meshes(bpy.context)
    before = {o.name: np.array([(o.matrix_world @ v.co)[:] for v in o.data.vertices]) for o in originals}
    bpy.context.scene.hitagi.bridge = str(ROOT / 'temp/usd-viewer/live')
    bpy.context.scene.hitagi.export_path = str(output / 'miku.usda')
    copies = authoring.prepare_copy(bpy.context)
    for obj in copies:
        name = obj['hitagi_prepared_from']
        after = np.array([(obj.matrix_world @ v.co)[:] for v in obj.data.vertices])
        assert np.allclose(before[name], after, atol=1e-6), f'Preparation changed geometry: {name}'
    # Keep originals in the editable project but hide them from its viewport.
    for obj in originals:
        obj.hide_set(True)
        obj.hide_render = True
    for obj in copies:
        for material in obj.data.materials:
            for image in (pipeline.base_material(material)[1], material.hitagi.control_image):
                if image and not image.packed_file:
                    image.pack()
    # Preserve attribution alongside the derivative asset, independently of ZIP.
    for name in ('readme.txt', 'readme_miku_v2.txt', 'Readme.three-mmd.txt', 'LICENSE.three-mmd.txt', 'provenance.json'):
        if (source / name).exists():
            shutil.copyfile(source / name, output / name)
    bpy.ops.wm.save_as_mainfile(filepath=str(output / 'miku.blend'))
    manifest = pipeline.export_asset(bpy.context, output / 'miku.usda', copies)
    assert manifest['triangles'] == 22961, manifest['triangles']
    assert len(manifest['textures']) >= 3
    summary = {'triangles': manifest['triangles'], 'meshes': len(manifest['meshes']),
               'textures': len(manifest['textures']), 'geometry_preserved': True,
               'output': str(output), 'blender': bpy.app.version_string}
    report = ROOT / 'temp/blender-hitagi/prepare.json'
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
