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

import bpy, bgl
from bpy.types import Header, Menu, Operator
import math
import mathutils
import bmesh
import fd

from bpy.props import (StringProperty,
                       BoolProperty,
                       IntProperty,
                       FloatProperty,
                       FloatVectorProperty,
                       BoolVectorProperty,
                       PointerProperty,
                       EnumProperty)
    
class OPS_select_object_by_name(Operator):
    bl_idname = "fd_object.select_object_by_name"
    bl_label = "Select Object"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name="Object Name")

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj = bpy.data.objects[self.object_name]
        bpy.ops.object.select_all(action='DESELECT')
        obj.hide = False
        obj.hide_select = False
        obj.select = True
        context.scene.objects.active = obj
        return {'FINISHED'}
    
class OPS_toggle_edit_mode(Operator):
    bl_idname = "fd_object.toggle_edit_mode"
    bl_label = "Toggle Edit Mode"
    bl_description = "This will toggle between object and edit mode"
    
    object_name = StringProperty(name="Object Name")

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj = bpy.data.objects[self.object_name]
        obj.hide = False
        obj.hide_select = False
        obj.select = True
        context.scene.objects.active = obj
        bpy.ops.object.editmode_toggle()
        return {'FINISHED'}
    
class OPS_unwrap_mesh(Operator):
    bl_idname = "fd_object.unwrap_mesh"
    bl_label = "Unwrap Mesh"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name="Object Name")
    
    def execute(self, context):
        obj = bpy.data.objects[self.object_name]
        context.scene.objects.active = obj
        if obj.mode == 'OBJECT':
            bpy.ops.object.editmode_toggle()
            
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=66, island_margin=0, user_area_weight=0)
        bpy.ops.object.editmode_toggle()
        return {'FINISHED'}
    
class OPS_add_material_slot(Operator):
    bl_idname = "fd_object.add_material_slot"
    bl_label = "Add Material Slot"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name="Object Name")
    
    def execute(self,context):
        obj = bpy.data.objects[self.object_name]
#         obj.mv.material_slot_col.add()
        override = {'active_object':obj,'object':obj}
        bpy.ops.object.material_slot_add(override)
        return{'FINISHED'}
    
class OPS_apply_hook_modifiers(Operator):
    bl_idname = "fd_object.apply_hook_modifiers"
    bl_label = "Apply Hook Modifiers"
    bl_options = {'UNDO'}

    object_name = StringProperty(name="Object Name")

    def execute(self, context):
        obj = bpy.data.objects[self.object_name]
        context.scene.objects.active = obj
        list_mod = []
        if obj:
            for mod in obj.modifiers:
                if mod.type == 'HOOK':
                    list_mod.append(mod.name)
                    
        for mod in list_mod:
            bpy.ops.object.modifier_apply(apply_as='DATA',modifier=mod)
            
        return {'FINISHED'}

class OPS_add_camera(Operator):
    bl_idname = "fd_object.add_camera"
    bl_label = "Add Camera"
    bl_options = {'UNDO'}

    def execute(self, context):
        bpy.ops.object.camera_add(view_align=False)
        camera = context.active_object
        bpy.ops.view3d.camera_to_view()
        camera.data.clip_start = 0
        camera.data.clip_end = 9999
        camera.data.ortho_scale = 200.0
        return {'FINISHED'}

class OPS_add_modifier(Operator):
    bl_idname = "fd_object.add_modifier"
    bl_label = "Add Modifier"
    bl_options = {'UNDO'}
    
    type = EnumProperty(items=fd.enums.enum_modifiers, name="Modifier Type")
    
    @classmethod
    def poll(cls, context):
        return True
    
    def execute(self, context):
        bpy.ops.object.modifier_add(type=self.type)
        return{'FINISHED'}    

class OPS_add_constraint(Operator):    
    bl_idname = "fd_object.add_constraint"
    bl_label = "Add Constraint"
    bl_options = {'UNDO'}
    
    type = EnumProperty(items=fd.enums.enum_constraints, name="Constraint Type")
    
    @classmethod
    def poll(cls, context):
        return True
    
    def execute(self, context):
        bpy.ops.object.constraint_add(type=self.type)
        return{'FINISHED'}    

class OPS_collapse_all_modifiers(Operator):
    bl_idname = "fd_object.collapse_all_modifiers"
    bl_label = "Collapse All Modifiers"
    bl_options = {'UNDO'}
    
    @classmethod
    def poll(cls, context):
        return context.active_object

    def execute(self, context):
        for mod in context.active_object.modifiers:
            mod.show_expanded = False
        return {'FINISHED'}
    
class OPS_collapse_all_constraints(Operator):
    bl_idname = "fd_object.collapse_all_constraints"
    bl_label = "Collapse All Constraints"
    bl_options = {'UNDO'}
    
    @classmethod
    def poll(cls, context):
        return context.active_object

    def execute(self, context):
        for con in context.active_object.constraints:
            con.show_expanded = False
        return {'FINISHED'}
    
class OPS_camera_properties(Operator):
    bl_idname = "fd_object.camera_properties"
    bl_label = "Camera Properties"

    lock_camera = BoolProperty(name="Lock Camera to View")

    @classmethod
    def poll(cls, context):
        return True

    def check(self,context):
        return True

    def __del__(self):
        if self.lock_camera == True:
            bpy.context.space_data.lock_camera = True
        else:
            bpy.context.space_data.lock_camera = False

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self,context,event):
        self.lock_camera = context.space_data.lock_camera
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
         obj = context.active_object
         fd.draw_object_info(self.layout,obj)
         fd.draw_object_data(self.layout, obj)
         self.layout.prop(self,'lock_camera')
         
        
#         if context.active_object.type == 'CAMERA':
#             obj = context.active_object
#             cam = obj.data
#             ccam = cam.cycles
#             layout = self.layout
#             box = layout.box()
#             name_box = box.box()
#             row = name_box.row()
#             split = row.split(percentage=0.2)
#             split.label("Name:",icon='SCENE')
#             split.prop(obj,"name",text="")
#             
#             box.label("Transform:")
#             box1 = box.box()
#             row = box1.row()
#             split = box1.split()
#             col = split.column()
#             row = col.row(align=True)          
#             row.prop(obj,"lock_location",index=0,text="")  
#             row.prop(obj,"location",index=0,text="X")
#             row = col.row(align=True)
#             row.prop(obj,"lock_location",index=1,text="")
#             row.prop(obj,"location",index=1,text="Y")
#             row = col.row(align=True)
#             row.prop(obj,"lock_location",index=2,text="")
#             row.prop(obj,"location",index=2,text="Z")
#             col = split.column()
#             row = col.row(align=True)
#             row.prop(obj,"lock_rotation",index=0,text="")
#             row.prop(obj,"rotation_euler",index=0,text="X")
#             row = col.row(align=True)
#             row.prop(obj,"lock_rotation",index=1,text="")
#             row.prop(obj,"rotation_euler",index=1,text="Y")
#             row = col.row(align=True)
#             row.prop(obj,"lock_rotation",index=2,text="")
#             row.prop(obj,"rotation_euler",index=2,text="Z")
#             
#             box.label("Camera Type:")           
#             cam_opt_box_1 = box.box()
#             row = cam_opt_box_1.row()
#             row.prop(cam, "type", expand=True, text="Camera Type")
#             split = cam_opt_box_1.split()
#             col = split.column()
#             if cam.type == 'PERSP':
#                 row = col.row()
#                 if cam.lens_unit == 'MILLIMETERS':
#                     row.prop(cam, "lens")
#                 elif cam.lens_unit == 'FOV':
#                     row.prop(cam, "angle")
#                 row.prop(cam, "lens_unit", text="")
#     
#             if cam.type == 'ORTHO':
#                 col.prop(cam, "ortho_scale")
#         
#             if cam.type == 'PANO':
#                 engine = bpy.context.scene.render.engine
#                 if engine == 'CYCLES':
#                     ccam = cam.cycles
#                     col.prop(ccam, "panorama_type", text="Panorama Type")
#                     if ccam.panorama_type == 'FISHEYE_EQUIDISTANT':
#                         col.prop(ccam, "fisheye_fov")
#                     elif ccam.panorama_type == 'FISHEYE_EQUISOLID':
#                         row = col.row()
#                         row.prop(ccam, "fisheye_lens", text="Lens")
#                         row.prop(ccam, "fisheye_fov")
#                 elif engine == 'BLENDER_RENDER':
#                     row = col.row()
#                     if cam.lens_unit == 'MILLIMETERS':
#                         row.prop(cam, "lens")
#                     elif cam.lens_unit == 'FOV':
#                         row.prop(cam, "angle")
#                     row.prop(cam, "lens_unit", text="")            
#                 
#             box.label("Camera Options:")
#             cam_opt_box_2 = box.box()
#             row = cam_opt_box_2.row()
# #             row.menu("CAMERA_MT_presets", text=bpy.types.CAMERA_MT_presets.bl_label) # THIS ERRORS OUT DUE TO CONTEXT  
#             row.prop_menu_enum(cam, "show_guide")            
#             row = cam_opt_box_2.row()
#             split = row.split()
#             col = split.column()
#             col.prop(cam, "clip_start", text="Clipping Start")
#             col.prop(cam, "clip_end", text="Clipping End")      
#             col = row.column()         
#             col.prop(bpy.context.scene.cycles,"film_transparent",text="Transparent Film")   
#             col.prop(self,"lock_camera",text="Lock Camera to View")
    
class OPS_create_floor_plane(Operator):
    bl_idname = "fd_object.create_floor_plane"
    bl_label = "Create Floor Plane"
    bl_options = {'UNDO'}
    
    def execute(self, context):
        largest_x = 0
        largest_y = 0
        smallest_x = 0
        smallest_y = 0
        wall_groups = []
        for obj in context.visible_objects:
            if obj.mv.type == 'BPWALL':
                wall_groups.append(fd.Wall(obj))
            
        for group in wall_groups:
            start_point = (group.obj_bp.matrix_world[0][3],group.obj_bp.matrix_world[1][3],0)
            end_point = (group.obj_x.matrix_world[0][3],group.obj_x.matrix_world[1][3],0)

            if start_point[0] > largest_x:
                largest_x = start_point[0]
            if start_point[1] > largest_y:
                largest_y = start_point[1]
            if start_point[0] < smallest_x:
                smallest_x = start_point[0]
            if start_point[1] < smallest_y:
                smallest_y = start_point[1]
            if end_point[0] > largest_x:
                largest_x = end_point[0]
            if end_point[1] > largest_y:
                largest_y = end_point[1]
            if end_point[0] < smallest_x:
                smallest_x = end_point[0]
            if end_point[1] < smallest_y:
                smallest_y = end_point[1]

        loc = (smallest_x , smallest_y,0)
        width = math.fabs(smallest_y) + math.fabs(largest_y)
        length = math.fabs(largest_x) + math.fabs(smallest_x)
        if width == 0:
            width = fd.inches(-48)
        if length == 0:
            length = fd.inches(-48)
        obj_plane = fd.create_floor_mesh('Floor',(length,width,0.0))
        obj_plane.location = loc
        return {'FINISHED'}
    
#------REGISTER
classes = [
           OPS_select_object_by_name,
           OPS_toggle_edit_mode,
           OPS_unwrap_mesh,
           OPS_add_material_slot,
           OPS_apply_hook_modifiers,
           OPS_add_camera,
           OPS_add_modifier,
           OPS_add_constraint,
           OPS_collapse_all_modifiers,
           OPS_collapse_all_constraints,
           OPS_camera_properties,
           OPS_create_floor_plane
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
