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

sys.path.insert(0, str(Path(__file__).parent))
import blender_hitagi
from blender_hitagi import pipeline, authoring

ROOT = Path(__file__).resolve().parents[2]


class PipelineTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False)
        self.work = tempfile.TemporaryDirectory(dir=ROOT / 'temp/blender-hitagi')
        self.path = Path(self.work.name) / 'asset.usda'
        bpy.context.scene.unit_settings.scale_length = 1
        bpy.context.scene.hitagi.head = None
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
        for name, kind in [('Face', 'FACE'), ('Body', 'SKIN')]:
            mat = bpy.data.materials.new(name)
            mat.use_nodes = True
            mat.hitagi.kind = kind
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
            head = mat.ComputeSurfaceSource()[0].GetInput('animeHead').Get()
            self.assertAlmostEqual(head[1], bpy.context.scene.hitagi.head_center.z * .1, places=5)

    def test_control_data_portability_and_import(self):
        pixels = np.zeros((8, 8, 4), dtype=np.float32)
        pixels[..., :] = (.2, .6, .8, .4)
        image = authoring.new_image('Raw control', pixels)
        self.materials[0].hitagi.control_image = image
        self.materials[0].hitagi.base_image = image
        self.materials[0].hitagi.base_tint = (.8, .9, 1, .7)
        head = bpy.data.objects.new('Head reference', None)
        bpy.context.collection.objects.link(head)
        head.matrix_world = Matrix.Translation((0, 0, 2)) @ Matrix.Rotation(.4, 4, 'Z')
        bpy.context.scene.hitagi.head = head
        bpy.context.view_layer.update()
        expected = pipeline.head_frame(bpy.context.scene)
        report = pipeline.export_asset(bpy.context, self.path)
        moved = Path(self.work.name) / 'moved'
        moved.mkdir()
        shutil.copyfile(self.path, moved / self.path.name)
        shutil.copytree(self.path.parent / 'textures', moved / 'textures')
        imported = pipeline.import_asset(bpy.context, moved / self.path.name)
        face = next(m for m in imported if m.hitagi.kind == 'FACE')
        actual = np.array(face.hitagi.control_image.pixels[:]).reshape(8, 8, 4)
        self.assertTrue(np.allclose(actual, pixels, atol=1/255))
        self.assertEqual(face.hitagi.control_image.colorspace_settings.name, 'Non-Color')
        self.assertTrue(np.allclose(face.hitagi.base_tint, (.8, .9, 1, .7)))
        actual_head = pipeline.head_frame(bpy.context.scene)
        self.assertTrue(np.allclose(expected[0], actual_head[0]))
        self.assertTrue(np.allclose(expected[1], actual_head[1]))
        self.assertTrue(np.allclose(expected[2], actual_head[2]))
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

    def test_prepare_preserves_original_and_geometry(self):
        self.obj.data.polygons[1].material_index = 0
        self.obj.data.materials.pop(index=1)
        before = np.array([v.co[:] for v in self.obj.data.vertices])
        normals = np.array([n.vector[:] for n in self.obj.data.corner_normals])
        material = self.obj.data.materials[0]
        copies = authoring.prepare_copy(bpy.context)
        self.assertEqual(len(copies), 1)
        self.assertTrue(np.array_equal(before, np.array([v.co[:] for v in copies[0].data.vertices])))
        self.assertTrue(np.array_equal(normals, np.array([n.vector[:] for n in self.obj.data.corner_normals])))
        self.assertIsNone(material.hitagi.control_image)
        self.assertIsNotNone(copies[0].data.materials[0].hitagi.control_image)


if __name__ == '__main__':
    (ROOT / 'temp/blender-hitagi').mkdir(parents=True, exist_ok=True)
    blender_hitagi.register()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(PipelineTest))
    blender_hitagi.unregister()
    blender_hitagi.register()
    blender_hitagi.unregister()
    (ROOT / 'temp/blender-hitagi/tests.json').write_text(json.dumps({'tests': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors), 'blender': bpy.app.version_string}))
    if not result.wasSuccessful():
        raise RuntimeError('Blender–Hitagi integration tests failed')
