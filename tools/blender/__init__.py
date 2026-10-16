"""Blender–Hitagi: static mesh import and USD asset export."""
import bpy
from bpy.props import FloatProperty, PointerProperty, StringProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper

from . import pipeline

bl_info = {'name': 'Blender–Hitagi', 'author': 'HitagiEngine contributors', 'version': (0, 1, 0),
           'blender': (4, 5, 0), 'location': '3D View > Sidebar > Hitagi', 'category': 'Import-Export'}


class HitagiMaterial(bpy.types.PropertyGroup):
    base_image: PointerProperty(name='Base Image Override', type=bpy.types.Image)
    alpha_cutoff: FloatProperty(name='Alpha Cutoff', default=0, min=0, max=1)


class HitagiScene(bpy.types.PropertyGroup):
    export_path: StringProperty(name='Asset', subtype='FILE_PATH', default='//hitagi/asset.usda')
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
        layout.operator('hitagi.validate_asset')
        layout.prop(settings, 'export_path')
        layout.operator('hitagi.export_asset')
        if settings.status:
            for line in settings.status.splitlines()[:5]:
                layout.label(text=line)


class HITAGI_PT_material(bpy.types.Panel):
    bl_label = 'Hitagi Material'
    bl_idname = 'HITAGI_PT_material'
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'material'

    @classmethod
    def poll(cls, context):
        return context.material is not None

    def draw(self, context):
        layout = self.layout
        settings = context.material.hitagi
        for prop in ('base_image', 'alpha_cutoff'):
            layout.prop(settings, prop)


CLASSES = (HitagiMaterial, HitagiScene, HITAGI_OT_import, HITAGI_OT_validate,
           HITAGI_OT_export, HITAGI_PT_asset, HITAGI_PT_material)


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
