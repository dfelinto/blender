# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy
from bpy.types import Header, Menu, Operator
import os, subprocess
from bpy.app.handlers import persistent

from bpy.props import (StringProperty,
                       BoolProperty,
                       IntProperty,
                       FloatProperty,
                       FloatVectorProperty,
                       PointerProperty,
                       EnumProperty)

class OPS_render_scene(Operator): # Used in space_fluid_info_header ln: 290,302
    bl_idname = "fd_scene.render_scene"
    bl_label = "Render Scene"
    
    def execute(self, context):
        ui = context.scene.mv.ui
        rd = context.scene.render
        rl = rd.layers.active
        freestyle_settings = rl.freestyle_settings        
#         scene = context.scene.mv
#         
#         for obj in context.scene.objects:
#             if obj.hide or obj.mv.type == 'CAGE':
#                 obj.cycles_visibility.camera = False
#                 obj.cycles_visibility.diffuse = False
#                 obj.cycles_visibility.glossy = False
#                 obj.cycles_visibility.transmissi0on = False
#                 obj.cycles_visibility.shadow = False
#             else:
#                 obj.cycles_visibility.camera = True
#                 obj.cycles_visibility.diffuse = True
#                 obj.cycles_visibility.glossy = True
#                 obj.cycles_visibility.transmission = True
#                 obj.cycles_visibility.shadow = True
        #render
        if ui.render_type_tabs == 'NPR':
            rd.engine = 'BLENDER_RENDER'
            rd.use_freestyle = True
            rd.line_thickness = 0.75
            rl.use_pass_combined = False
            rl.use_pass_z = False
            freestyle_settings.crease_angle = 2.617994
       
        bpy.ops.render.render('INVOKE_DEFAULT')
        return {'FINISHED'}
    
    @persistent
    def set_to_cycles_re(self):
        bpy.context.scene.render.engine = 'CYCLES'
        
    bpy.app.handlers.render_complete.append(set_to_cycles_re)

class OPS_render_settings(Operator): # Used in space_fluid_info_header ln: 294
    bl_idname = "fd_scene.render_settings"
    bl_label = "Render Settings"
    
    def execute(self, context):
        return {'FINISHED'}
    
    def check(self,context):
        return True
    
    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        scene = bpy.context.scene
        rd = scene.render
        image_settings = rd.image_settings
        ui = context.scene.mv.ui
        rl = rd.layers.active
        linestyle = rl.freestyle_settings.linesets[0].linestyle
        
        box = layout.box()
        row = box.row(align=True)
        row.prop_enum(ui,"render_type_tabs", 'PRR',icon='RENDER_STILL',text="Photo Realistic Render")
        row.prop_enum(ui,"render_type_tabs", 'NPR',icon='SNAP_FACE',text="Line Drawing")
        row = box.row(align=True)
        row.label(text="Render Size:",icon='STICKY_UVS_VERT')        
        row.prop(rd, "resolution_x", text="X")
        row.prop(rd, "resolution_y", text="Y")
        
        if ui.render_type_tabs == 'PRR':
            row = box.row()
            row.label(text="Rendering Quality:",icon='IMAGE_COL')
            row.prop(scene.cycles,"samples",text='Passes')
            row = box.row()
            row.label(text="Image Format:",icon='IMAGE_DATA')
            row.prop(image_settings,"file_format",text="")
            row = box.row()
            row.label(text="Display Mode:",icon='RENDER_RESULT')
            row.prop(rd,"display_mode",text="")
            row = box.row()
            row.label(text="Use Transparent Film:",icon='SEQUENCE')
            row.prop(scene.cycles,"film_transparent",text='')
            
        if ui.render_type_tabs == 'NPR':
            row = box.row()
            row.label(text="Image Format:",icon='IMAGE_DATA')
            row.prop(image_settings,"file_format",text="")
            row = box.row()
            row.label(text="Display Mode:",icon='RENDER_RESULT')
            row.prop(rd,"display_mode",text="")
            row = box.row()
            row.prop(linestyle, "color", text="Line Color")
            row = box.row()
            row.prop(bpy.data.worlds[0], "horizon_color", text="Background Color")
        
class OPS_add_thumbnail_camera_and_lighting(Operator):
    bl_idname = "fd_scene.add_thumbnail_camera_and_lighting"
    bl_label = "Add Thumbnail Camera"
    bl_options = {'UNDO'}

    def execute(self, context):
        bpy.ops.object.camera_add(view_align=True)
        camera = context.active_object
        context.scene.camera = camera
        context.scene.cycles.film_transparent = True
        
        camera.data.clip_end = 9999
        camera.scale = (10,10,10)
        
        bpy.ops.object.lamp_add(type='SUN')
        sun = bpy.context.object
        sun.select = False
        sun.rotation_euler = (.785398, .785398, 0.0)
        
        context.scene.render.resolution_x = 1080
        context.scene.render.resolution_y = 1080
        context.scene.render.resolution_percentage = 25
        
        override = {}
        for window in bpy.context.window_manager.windows:
            screen = window.screen
             
            for area in screen.areas:
                if area.type == 'VIEW_3D':
                    override = {'window': window, 'screen': screen, 'area': area}
                    bpy.ops.view3d.camera_to_view(override)
                    for space in area.spaces:
                        if space.type == 'VIEW_3D':
                            space.lock_camera = True
                    break
        return {'FINISHED'}
        
class OPS_create_unity_build(Operator): #Not Used
    bl_idname = "fd_scene.create_unity_build"
    bl_label = "Build for Unity 3D"
    bl_options = {'UNDO'}
    
    save_name = StringProperty(name="Name")
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        subprocess.call("\"C:\\Program Files (x86)\\Unity\\Editor\\Unity.exe\"" + \
                         "-batchmode -executeMethod PerformBuild.build", shell=False)
        return {'FINISHED'}        

#------REGISTER
classes = [
           OPS_render_scene,
           OPS_render_settings,
           OPS_add_thumbnail_camera_and_lighting,
           OPS_create_unity_build
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
