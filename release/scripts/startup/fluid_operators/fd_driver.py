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

import bpy,os

from bpy.types import (Header, 
                       Menu, 
                       Panel, 
                       Operator,
                       PropertyGroup)

from bpy.props import (StringProperty,
                       BoolProperty,
                       IntProperty,
                       FloatProperty,
                       FloatVectorProperty,
                       PointerProperty,
                       EnumProperty,
                       CollectionProperty)

import math
import fd

class fd_prompt_collection(PropertyGroup):
    add = BoolProperty(name="Add")
    type = StringProperty(name="Type")

bpy.utils.register_class(fd_prompt_collection)

class OPS_turn_on_driver(Operator):
    bl_idname = "fd_driver.turn_on_driver"
    bl_label = "Turn On Driver"
    bl_options = {'UNDO'}

    object_name = StringProperty(name="Object Name")
    group = None
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        ui = context.scene.mv.ui
        self.group = fd.Assembly(bpy.data.objects[self.object_name])
        self.group.obj_bp.select = True
        if ui.group_driver_tabs == 'LOC_X':
            self.group.obj_bp.driver_add('location',0)
        if ui.group_driver_tabs == 'LOC_Y':
            self.group.obj_bp.driver_add('location',1)
        if ui.group_driver_tabs == 'LOC_Z':
            self.group.obj_bp.driver_add('location',2)
        if ui.group_driver_tabs == 'ROT_X':
            self.group.obj_bp.driver_add('rotation_euler',0)
        if ui.group_driver_tabs == 'ROT_Y':
            self.group.obj_bp.driver_add('rotation_euler',1)
        if ui.group_driver_tabs == 'ROT_Z':
            self.group.obj_bp.driver_add('rotation_euler',2)
        if ui.group_driver_tabs == 'DIM_X':
            obj_x = self.group.obj_x
            obj_x.select = True
            obj_x.driver_add('location',0)
        if ui.group_driver_tabs == 'DIM_Y':
            obj_y = self.group.obj_y
            obj_y.select = True
            obj_y.driver_add('location',1)
        if ui.group_driver_tabs == 'DIM_Z':
            obj_z = self.group.obj_z
            obj_z.select = True
            obj_z.driver_add('location',2)
        if ui.group_driver_tabs == 'PROMPTS':
            prompt = self.group.obj_bp.mv.PromptPage.COL_Prompt[self.group.obj_bp.mv.PromptPage.PromptIndex]
            if prompt.Type == 'DISTANCE':
                self.group.obj_bp.driver_add('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].DistanceValue')
            if prompt.Type == 'ANGLE':
                self.group.obj_bp.driver_add('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].AngleValue')
            if prompt.Type == 'PERCENTAGE':
                self.group.obj_bp.driver_add('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].PercentageValue')
            if prompt.Type == 'PRICE':
                self.group.obj_bp.driver_add('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].PriceValue')
            if prompt.Type == 'NUMBER':
                self.group.obj_bp.driver_add('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].NumberValue')
            if prompt.Type == 'QUANTITY':
                self.group.obj_bp.driver_add('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].QuantityValue')
            if prompt.Type == 'COMBOBOX':
                self.group.obj_bp.driver_add('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].EnumIndex')
            if prompt.Type == 'CHECKBOX':
                self.group.obj_bp.driver_add('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].CheckBoxValue')
        return {'FINISHED'}

class OPS_turn_off_driver(Operator):
    bl_idname = "fd_driver.turn_off_driver"
    bl_label = "Turn Off Driver"
    bl_options = {'UNDO'}

    object_name = StringProperty(name="Object Name")
    group = None
    
    @classmethod
    def poll(cls, context):
        return True
    
    def execute(self, context):
        ui = context.scene.mv.ui
        self.group = fd.Assembly(bpy.data.objects[self.object_name])
        if ui.group_driver_tabs == 'LOC_X':
            self.group.obj_bp.driver_remove('location',0)
        if ui.group_driver_tabs == 'LOC_Y':
            self.group.obj_bp.driver_remove('location',1)
        if ui.group_driver_tabs == 'LOC_Z':
            self.group.obj_bp.driver_remove('location',2)
        if ui.group_driver_tabs == 'ROT_X':
            self.group.obj_bp.driver_remove('rotation_euler',0)
        if ui.group_driver_tabs == 'ROT_Y':
            self.group.obj_bp.driver_remove('rotation_euler',1)
        if ui.group_driver_tabs == 'ROT_Z':
            self.group.obj_bp.driver_remove('rotation_euler',2)
        if ui.group_driver_tabs == 'DIM_X':
            obj_x = self.group.obj_x
            obj_x.select = True
            obj_x.driver_remove('location',0)
        if ui.group_driver_tabs == 'DIM_Y':
            obj_y = self.group.obj_y
            obj_y.select = True
            obj_y.driver_remove('location',1)
        if ui.group_driver_tabs == 'DIM_Z':
            obj_z = self.group.obj_z
            obj_z.select = True
            obj_z.driver_remove('location',2)
        if ui.group_driver_tabs == 'PROMPTS':
            prompt = self.group.obj_bp.mv.PromptPage.COL_Prompt[self.group.obj_bp.mv.PromptPage.PromptIndex]
            if prompt.Type == 'DISTANCE':
                self.group.obj_bp.driver_remove('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].DistanceValue')
            if prompt.Type == 'ANGLE':
                self.group.obj_bp.driver_remove('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].AngleValue')
            if prompt.Type == 'PERCENTAGE':
                self.group.obj_bp.driver_remove('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].PercentageValue')
            if prompt.Type == 'PRICE':
                self.group.obj_bp.driver_remove('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].PriceValue')
            if prompt.Type == 'NUMBER':
                self.group.obj_bp.driver_remove('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].NumberValue')
            if prompt.Type == 'QUANTITY':
                self.group.obj_bp.driver_remove('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].QuantityValue')
            if prompt.Type == 'COMBOBOX':
                self.group.obj_bp.driver_remove('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].EnumIndex')
            if prompt.Type == 'CHECKBOX':
                self.group.obj_bp.driver_remove('mv.PromptPage.COL_Prompt["'+ prompt.name + '"].CheckBoxValue')
        return {'FINISHED'}

class OPS_add_variable_to_object(Operator):
    bl_idname = "fd_driver.add_variable_to_object"
    bl_label = "Add Variable"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name='Object Name')
    data_path = StringProperty(name='Data Path')
    array_index = IntProperty(name='Array Index')
    type = EnumProperty(name="Active Library Type",
                    items=[("OBJECT","Object","Object"),("SCENE","Scene","Scene")],
                    default='OBJECT')

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        scene = context.scene
        obj_bp = bpy.data.objects[self.object_name]
        for DR in obj_bp.animation_data.drivers:
            if self.data_path in DR.data_path and DR.array_index == self.array_index:
                var = DR.driver.variables.new()
                var.targets[0].id_type = 'OBJECT'
                if self.type == 'SCENE':
                    var.targets[0].id = scene    
                else:
                    var.targets[0].id = None                                         
                var.type = 'SINGLE_PROP'
                for target in var.targets:
                    target.transform_space = 'LOCAL_SPACE'
        return {'FINISHED'}

class OPS_get_vars_from_object(Operator):
    bl_idname = "fd_driver.get_vars_from_object"
    bl_label = "Quick Variables"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name='Object Name')
    var_object_name = StringProperty(name='Variable Object Name')
    data_path = StringProperty(name='Data Path')
    array_index = IntProperty(name='Array Index')
    
    x_dim = BoolProperty(name='X Dimension',default=False)
    y_dim = BoolProperty(name='Y Dimension',default=False)
    z_dim = BoolProperty(name='Z Dimension',default=False)
    x_loc = BoolProperty(name='X Location',default=False)
    y_loc = BoolProperty(name='Y Location',default=False)
    z_loc = BoolProperty(name='Z Location',default=False)
    x_rot = BoolProperty(name='X Rotation',default=False)
    y_rot = BoolProperty(name='Y Rotation',default=False)
    z_rot = BoolProperty(name='Z Rotation',default=False)
    spec_group_index = BoolProperty(name="Specification Group Index",default=False)
    
    COL_prompt = CollectionProperty(name='Collection Prompts',type=fd_prompt_collection)
    
    group = None
    
    @classmethod
    def poll(cls, context):
        return True

    def get_part_prompts(self):
        for old_prompt in self.COL_prompt:
            self.COL_prompt.remove(0)
        
        for prompt in self.group.obj_bp.mv.PromptPage.COL_Prompt:
            prompt_copy = self.COL_prompt.add()
            prompt_copy.name = prompt.name
            prompt_copy.type = prompt.Type

    def execute(self, context):
        obj = bpy.data.objects[self.object_name]
        obj_bp = fd.get_assembly_bp(obj)
        if obj_bp:
            self.group = fd.Assembly(obj_bp)
            for DR in obj.animation_data.drivers:
                if self.data_path in DR.data_path and DR.array_index == self.array_index:
                    DR.driver.show_debug_info = False
                    
                    if self.x_loc:
                        var = DR.driver.variables.new()
                        var.name = 'loc_x'
                        var.targets[0].id = self.group.obj_bp
                        var.targets[0].data_path = 'location.x'
                    if self.y_loc:
                        var = DR.driver.variables.new()
                        var.name = 'loc_y'
                        var.targets[0].id = self.group.obj_bp
                        var.targets[0].data_path = 'location.y'
                    if self.z_loc:
                        var = DR.driver.variables.new()
                        var.name = 'loc_z'
                        var.targets[0].id = self.group.obj_bp
                        var.targets[0].data_path = 'location.z'
                    if self.x_rot:
                        var = DR.driver.variables.new()
                        var.name = 'rot_x'
                        var.targets[0].id = self.group.obj_bp
                        var.targets[0].data_path = 'rotation_euler.x'
                    if self.y_rot:
                        var = DR.driver.variables.new()
                        var.name = 'rot_y'
                        var.targets[0].id = self.group.obj_bp
                        var.targets[0].data_path = 'rotation_euler.y'
                    if self.z_rot:
                        var = DR.driver.variables.new()
                        var.name = 'rot_z'
                        var.targets[0].id = self.group.obj_bp
                        var.targets[0].data_path = 'rotation_euler.z'
                    if self.x_dim:
                        var = DR.driver.variables.new()
                        var.name = 'dim_x'
                        var.targets[0].id = self.group.obj_x
                        var.targets[0].data_path = 'location.x'
                    if self.y_dim:
                        var = DR.driver.variables.new()
                        var.name = 'dim_y'
                        var.targets[0].id = self.group.obj_y
                        var.targets[0].data_path = 'location.y'
                    if self.z_dim:
                        var = DR.driver.variables.new()
                        var.name = 'dim_z'
                        var.targets[0].id = self.group.obj_z
                        var.targets[0].data_path = 'location.z'      
    #                 if self.spec_group_index:
    #                     var = DR.driver.variables.new()
    #                     var.name = 'sgi'
    #                     var.targets[0].id = obj_bp
    #                     var.targets[0].data_path = 'mv.spec_group_index'
                        
                    for prompt in self.COL_prompt:
                        if prompt.add:
                            var = DR.driver.variables.new()
                            var.name = prompt.name.replace(" ","")
                            var.targets[0].id = obj_bp
                            if prompt.type == 'NUMBER':
                                var.targets[0].data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].NumberValue'
                            if prompt.type == 'QUANTITY':
                                var.targets[0].data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].QuantityValue'
                            if prompt.type == 'NUMBER':
                                var.targets[0].data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].NumberValue'
                            if prompt.type == 'COMBOBOX':
                                var.targets[0].data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].EnumIndex'
                            if prompt.type == 'CHECKBOX':
                                var.targets[0].data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].CheckBoxValue'
                            if prompt.type == 'DISTANCE':
                                var.targets[0].data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].DistanceValue'
                            if prompt.type == 'PERCENTAGE':
                                var.targets[0].data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].PercentageValue'
                            if prompt.type == 'PRICE':
                                var.targets[0].data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].PriceValue'
                            if prompt.type == 'ANGLE':
                                var.targets[0].data_path = 'mv.PromptPage.COL_Prompt["' + prompt.name + '"].AngleValue'
                                
                    var.type = 'SINGLE_PROP'
                    for target in var.targets:
                        target.transform_space = 'LOCAL_SPACE'
                    
        return {'FINISHED'}

    def reset_variables(self):
        self.x_dim = False
        self.y_dim = False
        self.z_dim = False
        self.x_loc = False
        self.y_loc = False
        self.z_loc = False
        self.x_rot = False
        self.y_rot = False
        self.z_rot = False
        self.spec_group_index = False

    def invoke(self,context,event):
        self.reset_variables()
        obj = bpy.data.objects[self.object_name]
        obj_bp = fd.get_assembly_bp(obj)
        if obj_bp:
            self.group = fd.Assembly(obj_bp)
            self.get_part_prompts()
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        box = layout.box()
        box.label('Main Properties ' + self.group.obj_bp.mv.name_object)
        row = box.row()
        col = row.column()
        col.prop(self,'x_dim')
        col.prop(self,'y_dim')
        col.prop(self,'z_dim')
        col = row.column()
        col.prop(self,'x_loc')
        col.prop(self,'y_loc')
        col.prop(self,'z_loc')
        col = row.column()
        col.prop(self,'x_rot')
        col.prop(self,'y_rot')
        col.prop(self,'z_rot')
#         row = box.row()
#         row.prop(self,'spec_group_index')
        box = layout.box()
        box.label('Prompts')
        col = box.column(align=True)
        for prompt in self.COL_prompt:
            row = col.row()
            row.label(prompt.name)
            row.prop(prompt,'add')

class OPS_remove_variable(Operator):
    bl_idname = "fd_driver.remove_variable"
    bl_label = "Remove Variable"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name='Object Name')
    data_path = StringProperty(name='Data Path')
    var_name = StringProperty(name='Variable Name')
    array_index = IntProperty(name='Array Index')

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj = bpy.data.objects[self.object_name]
        for DR in obj.animation_data.drivers:
            if DR.data_path == self.data_path:
                if DR.array_index == self.array_index:
                    for var in DR.driver.variables:
                        if var.name == self.var_name:
                            DR.driver.variables.remove(var)
        return {'FINISHED'}

#------REGISTER
classes = [
           OPS_turn_on_driver,
           OPS_turn_off_driver,
           OPS_add_variable_to_object,
           OPS_remove_variable,
           OPS_get_vars_from_object
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
