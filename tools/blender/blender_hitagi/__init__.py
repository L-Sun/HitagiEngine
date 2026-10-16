"""Blender–Hitagi: static character authoring and USD asset export."""
import bpy
from bpy.props import EnumProperty, FloatProperty, FloatVectorProperty, IntProperty, PointerProperty, StringProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper

from . import pipeline, authoring

bl_info = {'name': 'Blender–Hitagi', 'author': 'HitagiEngine contributors', 'version': (0, 1, 0),
           'blender': (4, 5, 0), 'location': '3D View > Sidebar > Hitagi', 'category': 'Import-Export'}


class HitagiMaterial(bpy.types.PropertyGroup):
    kind: EnumProperty(name='Surface', items=[(x, x.title(), '') for x in pipeline.KINDS])
    base_image: PointerProperty(name='Base Image Override', type=bpy.types.Image)
    base_tint: FloatVectorProperty(name='Base Tint', subtype='COLOR', size=4, default=(1, 1, 1, 1), min=0, max=1)
    control_image: PointerProperty(name='Face / Hair Control', type=bpy.types.Image)
    alpha_cutoff: FloatProperty(name='Alpha Cutoff', default=0, min=0, max=1)


class HitagiScene(bpy.types.PropertyGroup):
    head: PointerProperty(name='Head Reference', type=bpy.types.Object, description='+X right, +Y forward, +Z up; location is face center')
    head_center: FloatVectorProperty(name='Face Center', subtype='TRANSLATION', default=(0, 0, 1.448))
    face_width: FloatProperty(name='Face Half Width', default=.116, min=.0001)
    normal_strength: FloatProperty(name='Normal Smoothing', default=.45, min=0, max=1)
    cheek: FloatProperty(name='Cheek Light Bias', default=.08, min=-.3, max=.3)
    nose: FloatProperty(name='Nose Shadow Bias', default=.035, min=0, max=.3)
    map_size: IntProperty(name='Control Map Size', default=512, min=64, max=2048)
    export_path: StringProperty(name='Asset', subtype='FILE_PATH', default='//hitagi/character.usda')
    bridge: StringProperty(name='Viewer Bridge', subtype='DIR_PATH')
    status: StringProperty(name='Last Result')


class HITAGI_OT_import(bpy.types.Operator, ImportHelper):
    bl_idname = 'hitagi.import_asset'
    bl_label = 'Import Hitagi USD'
    bl_options = {'REGISTER', 'UNDO'}
    filename_ext = '.usda'
    filter_glob: StringProperty(default='*.usd;*.usda;*.usdc', options={'HIDDEN'})

    def execute(self, context):
        try:
            pipeline.import_asset(context, self.filepath)
            return {'FINISHED'}
        except Exception as exc:
            self.report({'ERROR'}, str(exc))
            return {'CANCELLED'}


class HITAGI_OT_prepare(bpy.types.Operator):
    bl_idname = 'hitagi.prepare_copy'
    bl_label = 'Prepare Anime Copy'
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        try:
            copies = authoring.prepare_copy(context)
            context.scene.hitagi.status = f'Prepared {len(copies)} meshes; originals preserved'
            self.report({'INFO'}, context.scene.hitagi.status)
            return {'FINISHED'}
        except Exception as exc:
            self.report({'ERROR'}, str(exc))
            return {'CANCELLED'}


class HITAGI_OT_validate(bpy.types.Operator):
    bl_idname = 'hitagi.validate_asset'
    bl_label = 'Validate Selection'

    def execute(self, context):
        report = pipeline.validate(context, pipeline.selected_meshes(context))
        context.scene.hitagi.status = '\n'.join(report['errors'] + report['warnings']) or 'Validation passed'
        self.report({'ERROR'} if report['errors'] else {'INFO'}, context.scene.hitagi.status)
        return {'CANCELLED'} if report['errors'] else {'FINISHED'}


class HITAGI_OT_export(bpy.types.Operator, ExportHelper):
    bl_idname = 'hitagi.export_asset'
    bl_label = 'Export Hitagi Asset'
    filename_ext = '.usda'
    filter_glob: StringProperty(default='*.usda', options={'HIDDEN'})

    def execute(self, context):
        try:
            report = pipeline.export_asset(context, self.filepath)
            context.scene.hitagi.export_path = self.filepath
            context.scene.hitagi.status = f"Exported {report['triangles']} triangles, {len(report['textures'])} textures"
            self.report({'INFO'}, context.scene.hitagi.status)
            return {'FINISHED'}
        except Exception as exc:
            self.report({'ERROR'}, str(exc))
            return {'CANCELLED'}

    def invoke(self, context, event):
        self.filepath = bpy.path.abspath(context.scene.hitagi.export_path)
        return ExportHelper.invoke(self, context, event)


class HITAGI_OT_send(bpy.types.Operator):
    bl_idname = 'hitagi.send_asset'
    bl_label = 'Export and Reload in Hitagi'

    def execute(self, context):
        try:
            path = bpy.path.abspath(context.scene.hitagi.export_path)
            report = pipeline.export_asset(context, path)
            pipeline.send_to_viewer(bpy.path.abspath(context.scene.hitagi.bridge), path)
            context.scene.hitagi.status = f"Viewer loaded {report['triangles']} triangles"
            self.report({'INFO'}, context.scene.hitagi.status)
            return {'FINISHED'}
        except Exception as exc:
            self.report({'ERROR'}, str(exc))
            return {'CANCELLED'}


class HITAGI_PT_asset(bpy.types.Panel):
    bl_label = 'Blender–Hitagi'
    bl_idname = 'HITAGI_PT_asset'
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Hitagi'

    def draw(self, context):
        layout = self.layout
        settings = context.scene.hitagi
        layout.operator('hitagi.import_asset')
        layout.label(text='Selected meshes / current pose')
        layout.prop(settings, 'head')
        if not settings.head:
            layout.prop(settings, 'head_center')
        layout.prop(settings, 'face_width')
        layout.prop(settings, 'normal_strength')
        layout.prop(settings, 'cheek')
        layout.prop(settings, 'nose')
        layout.prop(settings, 'map_size')
        layout.operator('hitagi.prepare_copy')
        layout.operator('hitagi.validate_asset')
        layout.prop(settings, 'export_path')
        layout.operator('hitagi.export_asset')
        layout.prop(settings, 'bridge')
        layout.operator('hitagi.send_asset')
        if settings.status:
            for line in settings.status.splitlines()[:5]:
                layout.label(text=line)


class HITAGI_PT_material(bpy.types.Panel):
    bl_label = 'Hitagi Anime Material'
    bl_idname = 'HITAGI_PT_anime_material'
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'material'

    @classmethod
    def poll(cls, context):
        return context.material is not None

    def draw(self, context):
        layout = self.layout
        settings = context.material.hitagi
        for prop in ('kind', 'base_image', 'base_tint', 'control_image', 'alpha_cutoff'):
            layout.prop(settings, prop)
        if settings.kind == 'FACE':
            layout.label(text='Control: R/G side thresholds, B hair shadow')
        elif settings.kind == 'HAIR':
            layout.label(text='Control: RG direction, B blend, A strength')


CLASSES = (HitagiMaterial, HitagiScene, HITAGI_OT_import, HITAGI_OT_prepare, HITAGI_OT_validate,
           HITAGI_OT_export, HITAGI_OT_send, HITAGI_PT_asset, HITAGI_PT_material)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Material.hitagi = PointerProperty(type=HitagiMaterial)
    bpy.types.Scene.hitagi = PointerProperty(type=HitagiScene)


def unregister():
    del bpy.types.Scene.hitagi
    del bpy.types.Material.hitagi
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
