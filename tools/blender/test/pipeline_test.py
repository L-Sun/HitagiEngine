"""Real Blender integration checks; no mocked bpy or generated expected exports."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

import bpy
from mathutils import Matrix, Vector
import numpy as np
from pxr import Usd, UsdGeom, UsdShade

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import blender
from blender import pipeline

ROOT = Path(__file__).resolve().parents[3]


class PipelineTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False)
        self.work = tempfile.TemporaryDirectory(dir=ROOT / 'temp/blender-hitagi')
        self.path = Path(self.work.name) / 'asset.usda'
        bpy.context.scene.unit_settings.scale_length = 1
        mesh = bpy.data.meshes.new('UV seam mesh')
        mesh.from_pydata([(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)], [], [(0, 1, 2), (0, 2, 3)])
        mesh.update()
        uv = mesh.uv_layers.new(name='UVMap')
        values = [(0, 0), (1, 0), (1, 1), (.2, .1), (.8, .9), (0, 1)]
        for loop, value in zip(uv.data, values):
            loop.uv = value
        normals = [(0, 0, 1)] * 3 + [(.1, 0, .994987)] * 3
        mesh.normals_split_custom_set(normals)
        self.obj = bpy.data.objects.new('真实网格', mesh)
        bpy.context.collection.objects.link(self.obj)
        self.obj.select_set(True)
        bpy.context.view_layer.objects.active = self.obj
        self.materials = []
        for name in ('MaterialA', 'MaterialB'):
            mat = bpy.data.materials.new(name)
            mat.use_nodes = True
            mesh.materials.append(mat)
            self.materials.append(mat)
        mesh.polygons[1].material_index = 1
        bpy.context.view_layer.update()

    def tearDown(self):
        self.work.cleanup()

    def test_evaluated_transform_normals_uv_and_materials(self):
        self.obj.matrix_world = Matrix.Translation((2, -1, 3)) @ Matrix.Diagonal((-2, 3, .5, 1))
        bpy.context.scene.unit_settings.scale_length = .1
        bpy.context.view_layer.update()
        report = pipeline.export_asset(bpy.context, self.path)
        self.assertEqual(report['triangles'], 2)
        self.assertEqual(len(report['meshes']), 2)
        stage = Usd.Stage.Open(str(self.path))
        meshes = [UsdGeom.Mesh(p) for p in stage.Traverse() if p.IsA(UsdGeom.Mesh)]
        for i, mesh in enumerate(meshes):
            indices = list(mesh.GetFaceVertexIndicesAttr().Get())
            self.assertEqual(indices, [2, 1, 0] if i == 0 else [3, 2, 0])
            points = mesh.GetPointsAttr().Get()
            expected = .1 * (pipeline.AXIS @ self.obj.matrix_world @ self.obj.data.vertices[indices[0]].co)
            self.assertTrue(np.allclose(points[indices[0]], expected))
            uv = UsdGeom.PrimvarsAPI(mesh).GetPrimvar('st')
            self.assertEqual(uv.GetInterpolation(), 'faceVarying')
            self.assertTrue(np.allclose(uv.Get()[0], (1, 1) if i == 0 else (0, 1)))
            self.assertEqual(mesh.GetNormalsInterpolation(), 'faceVarying')
            a, b, c = [Vector(points[j]) for j in indices]
            self.assertGreater((b-a).cross(c-a).normalized().dot(Vector(mesh.GetNormalsAttr().Get()[0])), .99)
            mat = UsdShade.MaterialBindingAPI(mesh).ComputeBoundMaterial()[0]
            self.assertTrue(mat)
            self.assertEqual(mat.ComputeSurfaceSource()[0].GetIdAttr().Get(), 'UsdPreviewSurface')

    def test_texture_portability_and_import(self):
        image = bpy.data.images.new('Base color', width=8, height=8, alpha=True)
        image.pixels[:] = [.2, .6, .8, 1] * 64
        self.materials[0].hitagi.base_image = image
        report = pipeline.export_asset(bpy.context, self.path)
        moved = Path(self.work.name) / 'moved'
        moved.mkdir()
        shutil.copyfile(self.path, moved / self.path.name)
        shutil.copytree(self.path.parent / 'textures', moved / 'textures')
        # Remove the source package so resolution must use the moved textures.
        self.path.unlink()
        shutil.rmtree(self.path.parent / 'textures')
        imported = pipeline.import_asset(bpy.context, moved / self.path.name)
        images = [n.image for m in imported if m.use_nodes for n in m.node_tree.nodes
                  if n.type == 'TEX_IMAGE' and n.image]
        self.assertTrue(images)
        self.assertTrue(all(Path(bpy.path.abspath(i.filepath)).is_file() for i in images))
        for path, digest in report['textures'].items():
            self.assertEqual(hashlib.sha256((moved / path).read_bytes()).hexdigest(), digest)

    def test_failure_preserves_previous_asset(self):
        pipeline.export_asset(bpy.context, self.path)
        before = self.path.read_bytes()
        self.obj.data.uv_layers.remove(self.obj.data.uv_layers.active)
        with self.assertRaisesRegex(ValueError, 'missing primary UV'):
            pipeline.export_asset(bpy.context, self.path)
        self.assertEqual(before, self.path.read_bytes())

    def test_export_operator_and_reregistration(self):
        result = bpy.ops.hitagi.export_asset(filepath=str(self.path))
        self.assertEqual(result, {'FINISHED'})
        self.assertTrue(self.path.exists())
        self.assertEqual(bpy.context.scene.hitagi.export_path, str(self.path))

    def test_unsupported_shader_fails_explicitly(self):
        mat = self.materials[0]
        bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        noise = mat.node_tree.nodes.new('ShaderNodeTexNoise')
        mat.node_tree.links.new(noise.outputs['Color'], bsdf.inputs['Base Color'])
        with self.assertRaisesRegex(ValueError, 'bake the Base Color'):
            pipeline.export_asset(bpy.context, self.path)
        self.assertFalse(self.path.exists())

    def test_modifier_evaluated_not_original_topology(self):
        solid = self.obj.modifiers.new('Thickness', 'SOLIDIFY')
        solid.thickness = .1
        report = pipeline.export_asset(bpy.context, self.path)
        self.assertEqual(report['triangles'], 12)
        self.assertTrue(any('evaluated pose' in text for text in report['warnings']))

if __name__ == '__main__':
    (ROOT / 'temp/blender-hitagi').mkdir(parents=True, exist_ok=True)
    blender.register()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(PipelineTest))
    blender.unregister()
    blender.register()
    blender.unregister()
    (ROOT / 'temp/blender-hitagi/tests.json').write_text(json.dumps({'tests': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors), 'blender': bpy.app.version_string}))
    if not result.wasSuccessful():
        raise RuntimeError('Blender–Hitagi integration tests failed')
