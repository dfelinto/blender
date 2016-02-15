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

from bpy.props import (StringProperty,
                       BoolProperty,
                       IntProperty,
                       FloatProperty,
                       FloatVectorProperty,
                       BoolVectorProperty,
                       PointerProperty,
                       EnumProperty)

import fd

class OPS_add_prompt(Operator):
    bl_idname = "fd_prompts.add_prompt"
    bl_label = "Add Prompt"
    bl_options = {'UNDO'}
    
    prompt_name = StringProperty(name="Prompt Name",default = "New Prompt")
    prompt_type = EnumProperty(name="Prompt Type",items=fd.enums.enum_prompt_types)
    data_name = StringProperty(name="Data Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        if self.data_type == 'OBJECT':
            obj = bpy.data.objects[self.data_name]
            if self.prompt_name not in obj.mv.PromptPage.COL_Prompt:
                prompt = obj.mv.PromptPage.add_prompt(self.prompt_name,self.prompt_type,obj.name)
                prompt.TabIndex = obj.mv.PromptPage.MainTabIndex
            else:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',message='Prompt Already Exists')
        elif self.data_type == 'SCENE':
            obj = bpy.data.scenes[self.data_name]
            if self.prompt_name not in obj.mv.PromptPage.COL_Prompt:
                prompt = obj.mv.PromptPage.add_prompt(self.prompt_name,self.prompt_type,obj.name)
                prompt.TabIndex = obj.mv.PromptPage.MainTabIndex
            else:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',message='Prompt Already Exists')
        elif self.data_type == 'MATERIAL':
            obj = bpy.data.materials[self.data_name]
            if self.prompt_name not in obj.mv.PromptPage.COL_Prompt:
                prompt = obj.mv.PromptPage.add_prompt(self.prompt_name,self.prompt_type,obj.name)
                prompt.TabIndex = obj.mv.PromptPage.MainTabIndex
            else:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',message='Prompt Already Exists')
        elif self.data_type == 'WORLD':
            obj = bpy.data.worlds[self.data_name]
            if self.prompt_name not in obj.mv.PromptPage.COL_Prompt:
                prompt = obj.mv.PromptPage.add_prompt(self.prompt_name,self.prompt_type,obj.name)
                prompt.TabIndex = obj.mv.PromptPage.MainTabIndex
            else:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',message='Prompt Already Exists')
        return {'FINISHED'}

    def invoke(self,context,event):
        self.prompt_name = "New Prompt"
        data = bpy.data.objects[self.data_name]
        Counter = 1
        while self.prompt_name + " " + str(Counter) in data.mv.PromptPage.COL_Prompt:
            Counter += 1
        self.prompt_name = self.prompt_name + " " + str(Counter)
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)
    
    def draw(self, context):
        layout = self.layout
        layout.prop(self,"prompt_name")
        layout.prop(self,"prompt_type")

class OPS_add_calculation_prompt(Operator):
    bl_idname = "fd_prompts.add_calculation_prompt"
    bl_label = "Add Calculation Prompt"
    bl_options = {'UNDO'}
    
    prompt_name = StringProperty(name="Prompt Name",default = "New Prompt")
    prompt_type = EnumProperty(name="Prompt Type",items=fd.enums.enum_prompt_types)
    data_name = StringProperty(name="Data Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        if self.data_type == 'OBJECT':
            obj = bpy.data.objects[self.data_name]
            if self.prompt_name not in obj.mv.PromptPage.COL_Prompt:
                prompt = obj.mv.PromptPage.add_prompt(self.prompt_name,'DISTANCE',obj.name)
                prompt.TabIndex = obj.mv.PromptPage.MainTabIndex
            else:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',message='Prompt Already Exists')
        elif self.data_type == 'SCENE':
            obj = bpy.data.scenes[self.data_name]
            if self.prompt_name not in obj.mv.PromptPage.COL_Prompt:
                prompt = obj.mv.PromptPage.add_prompt(self.prompt_name,'DISTANCE',obj.name)
                prompt.TabIndex = obj.mv.PromptPage.MainTabIndex
            else:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',message='Prompt Already Exists')
        elif self.data_type == 'MATERIAL':
            obj = bpy.data.materials[self.data_name]
            if self.prompt_name not in obj.mv.PromptPage.COL_Prompt:
                prompt = obj.mv.PromptPage.add_prompt(self.prompt_name,'DISTANCE',obj.name)
                prompt.TabIndex = obj.mv.PromptPage.MainTabIndex
            else:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',message='Prompt Already Exists')
        elif self.data_type == 'WORLD':
            obj = bpy.data.worlds[self.data_name]
            if self.prompt_name not in obj.mv.PromptPage.COL_Prompt:
                prompt = obj.mv.PromptPage.add_prompt(self.prompt_name,'DISTANCE',obj.name)
                prompt.TabIndex = obj.mv.PromptPage.MainTabIndex
            else:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',message='Prompt Already Exists')
        return {'FINISHED'}

    def invoke(self,context,event):
        self.prompt_name = "New Prompt"
        data = bpy.data.objects[self.data_name]
        Counter = 1
        while self.prompt_name + " " + str(Counter) in data.mv.PromptPage.COL_Prompt:
            Counter += 1
        self.prompt_name = self.prompt_name + " " + str(Counter)
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)
    
    def draw(self, context):
        layout = self.layout
        layout.prop(self,"prompt_name")

class OPS_run_calculator(Operator):
    bl_idname = "fd_prompts.run_calculator"
    bl_label = "Run Calculator"
    bl_options = {'UNDO'}
    
    tab_index = IntProperty(name="Tab Index")
    data_name = StringProperty(name="Data Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    
    @classmethod
    def poll(cls, context):
        return True
    
    def get_size(self,tab,obj):
        for child in obj.children:
            if tab.calculator_type == 'XDIM' and child.mv.type == 'VPDIMX':
                return child.location.x
            if tab.calculator_type == 'YDIM' and child.mv.type == 'VPDIMY':
                return child.location.y
            if tab.calculator_type == 'ZDIM' and child.mv.type == 'VPDIMZ':
                return child.location.z
            
    def execute(self, context):
        if self.data_type == 'OBJECT':
            obj = bpy.data.objects[self.data_name]
            tab = obj.mv.PromptPage.COL_MainTab[self.tab_index]
            size = self.get_size(tab, obj)
            non_equal_prompts_total_value = 0
            equal_prompt_qty = 0
            calc_prompts = []
            for prompt in obj.mv.PromptPage.COL_Prompt:
                if prompt.TabIndex == self.tab_index:
                    if prompt.equal:
                        equal_prompt_qty += 1
                        calc_prompts.append(prompt)
                    else:
                        non_equal_prompts_total_value += prompt.DistanceValue
                        
            if equal_prompt_qty > 0:
                prompt_value = (size - tab.calculator_deduction - non_equal_prompts_total_value) / equal_prompt_qty
                
                for prompt in calc_prompts:
                    prompt.DistanceValue = prompt_value
                    
                obj.location = obj.location
                
        return {'FINISHED'}

class OPS_delete_prompt(Operator): 
    bl_idname = "fd_prompts.delete_prompt"
    bl_label = "Delete Prompt"
    bl_options = {'UNDO'}
    
    prompt_name = StringProperty(name="Prompt Name",default = "New Prompt")
    data_name = StringProperty(name="Data Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        if self.data_type == 'OBJECT':
            obj = bpy.data.objects[self.data_name]
            obj.mv.PromptPage.delete_prompt(self.prompt_name)
        elif self.data_type == 'SCENE':
            obj = bpy.data.scenes[self.data_name]
            obj.mv.PromptPage.delete_prompt(self.prompt_name)
        elif self.data_type == 'MATERIAL':
            obj = bpy.data.materials[self.data_name]
            obj.mv.PromptPage.delete_prompt(self.prompt_name)
        elif self.data_type == 'WORLD':
            obj = bpy.data.worlds[self.data_name]
            obj.mv.PromptPage.delete_prompt(self.prompt_name)
        return {'FINISHED'}

    def invoke(self,context,event):    
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)

    def draw(self, context):
        layout = self.layout
        layout.label("Are you sure you want to delete the prompt?")

class OPS_delete_main_tab(Operator):
    bl_idname = "fd_prompts.delete_main_tab"
    bl_label = "Delete Main Tab"
    bl_options = {'UNDO'}

    data_name = StringProperty(name="Object Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        if self.data_type == 'OBJECT':
            data = bpy.data.objects[self.data_name]
            data.mv.PromptPage.delete_selected_tab()
        elif self.data_type == 'SCENE':
            data = bpy.data.scenes[self.data_name]
            data.mv.PromptPage.delete_selected_tab()
        elif self.data_type == 'MATERIAL':
            data = bpy.data.materials[self.data_name]
            data.mv.PromptPage.delete_selected_tab()
        elif self.data_type == 'WORLD':
            data = bpy.data.worlds[self.data_name]
            data.mv.PromptPage.delete_selected_tab()
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)

    def draw(self, context):
        layout = self.layout
        layout.label("Are you sure you want to delete the selected tab?")

class OPS_rename_prompt(Operator):
    bl_idname = "fd_prompts.rename_prompt"
    bl_label = "Rename Prompt"

    data_name = StringProperty(name="Data Name")
    old_name = StringProperty(name="Old Name",default="Old Name")
    new_name = StringProperty(name="New Name",default="Enter New Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj = bpy.data.objects[self.data_name]
        if self.new_name not in obj.mv.PromptPage.COL_Prompt:
            prompt = obj.mv.PromptPage.COL_Prompt[self.old_name]
            prompt.name = self.new_name
            for obj1 in bpy.data.objects:
                if obj1.animation_data:
                    for driver in obj1.animation_data.drivers:
                        for var in driver.driver.variables:
                            for target in var.targets:
                                if target.id == obj:
                                    if target.data_path.find(self.old_name) != -1:
                                        renamed_data_path = target.data_path.replace(self.old_name,self.new_name)
                                        target.data_path = renamed_data_path
        return {'FINISHED'}

    def invoke(self,context,event):    
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)

    def draw(self, context):
        layout = self.layout
        layout.prop(self,"new_name")

class OPS_rename_main_tab(Operator):
    bl_idname = "fd_prompts.rename_main_tab"
    bl_label = "Rename Main Tab"

    data_name = StringProperty(name="Data Name")
    new_name = StringProperty(name="New Name",default="Enter New Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        if self.data_type == 'OBJECT':
            data = bpy.data.objects[self.data_name]
            data.mv.PromptPage.rename_selected_tab(self.new_name)
        elif self.data_type == 'SCENE':
            data = bpy.data.scenes[self.data_name]
            data.mv.PromptPage.rename_selected_tab(self.new_name)
        elif self.data_type == 'MATERIAL':
            data = bpy.data.materials[self.data_name]
            data.mv.PromptPage.rename_selected_tab(self.new_name)
        elif self.data_type == 'WORLD':
            data = bpy.data.worlds[self.data_name]
            data.mv.PromptPage.rename_selected_tab(self.new_name)
        return {'FINISHED'}

    def invoke(self,context,event):    
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)

    def draw(self, context):
        layout = self.layout
        layout.prop(self,"new_name")

class OPS_show_prompt_properties(Operator):
    bl_idname = "fd_prompts.show_prompt_properties"
    bl_label = "Show Prompt Properties"
    
    prompt_name = StringProperty(name="Prompt Name",default = "New Prompt")
    prompt_type = EnumProperty(name="Prompt Type",items=fd.enums.enum_prompt_types)
    data_name = StringProperty(name="Data Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)

    def draw(self, context):
        layout = self.layout
        if self.data_type == 'OBJECT':
            data = bpy.data.objects[self.data_name]
            data.mv.PromptPage.COL_Prompt[self.prompt_name].draw_prompt_properties(layout,data.name)
        elif self.data_type == 'SCENE':
            data = bpy.data.scenes[self.data_name]
            data.mv.PromptPage.COL_Prompt[self.prompt_name].draw_prompt_properties(layout,data.name)
        elif self.data_type == 'MATERIAL':
            data = bpy.data.materials[self.data_name]
            data.mv.PromptPage.COL_Prompt[self.prompt_name].draw_prompt_properties(layout,data.name)
        elif self.data_type == 'WORLD':
            data = bpy.data.worlds[self.data_name]
            data.mv.PromptPage.COL_Prompt[self.prompt_name].draw_prompt_properties(layout,data.name)
            
class OPS_add_main_tab(Operator):
    bl_idname = "fd_prompts.add_main_tab"
    bl_label = "Add Main Tab"
    bl_options = {'UNDO'}
    
    tab_name = StringProperty(name="Tab Name",default="New Tab")
    tab_type = EnumProperty(name="Tab Type",items=fd.enums.enum_prompt_tab_types,default='VISIBLE')
    data_name = StringProperty(name="Data Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        if self.data_type == 'OBJECT':
            data = bpy.data.objects[self.data_name]
            data.mv.PromptPage.add_tab(self.tab_name,self.tab_type)
        elif self.data_type == 'SCENE':
            data = bpy.data.scenes[self.data_name]
            data.mv.PromptPage.add_tab(self.tab_name,self.tab_type)
        elif self.data_type == 'MATERIAL':
            data = bpy.data.materials[self.data_name]
            data.mv.PromptPage.add_tab(self.tab_name,self.tab_type)
        elif self.data_type == 'WORLD':
            data = bpy.data.worlds[self.data_name]
            data.mv.PromptPage.add_tab(self.tab_name,self.tab_type)
        return {'FINISHED'}

    def invoke(self,context,event):
        self.tab_name = "New Tab"
        if self.data_type == 'OBJECT':
            data = bpy.data.objects[self.data_name]
            Counter = 1
            while self.tab_name + " " + str(Counter) in data.mv.PromptPage.COL_MainTab:
                Counter += 1
            self.tab_name = self.tab_name + " " + str(Counter)
            
        elif self.data_type == 'SCENE':
            data = bpy.data.scenes[self.data_name]
            Counter = 1
            while self.tab_name + " " + str(Counter) in data.mv.PromptPage.COL_MainTab:
                Counter += 1
            self.tab_name = self.tab_name + " " + str(Counter)
            
        elif self.data_type == 'MATERIAL':
            data = bpy.data.materials[self.data_name]
            Counter = 1
            while self.tab_name + " " + str(Counter) in data.mv.PromptPage.COL_MainTab:
                Counter += 1
            self.tab_name = self.tab_name + " " + str(Counter)
            
        elif self.data_type == 'WORLD':
            data = bpy.data.worlds[self.data_name]
            Counter = 1
            while self.tab_name + " " + str(Counter) in data.mv.PromptPage.COL_MainTab:
                Counter += 1
            self.tab_name = self.tab_name + " " + str(Counter)
            
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)

    def draw(self, context):
        layout = self.layout
        layout.prop(self,"tab_name")
        layout.prop(self,"tab_type")
        
class OPS_add_combo_box_option(Operator): 
    bl_idname = "fd_prompts.add_combo_box_option"
    bl_label = "Add Combo Box Option"
    bl_options = {'UNDO'}
    
    data_name = StringProperty(name="Data Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    prompt_name = StringProperty(name="Prompt Name")
    combo_box_value = StringProperty(name="Combo Box Value",default = "Option")

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        if self.data_type == 'OBJECT':
            data = bpy.data.objects[self.data_name]
            data.mv.PromptPage.COL_Prompt[self.prompt_name].add_enum_item(self.combo_box_value)
        elif self.data_type == 'SCENE':
            data = bpy.data.scenes[self.data_name]
            data.mv.PromptPage.COL_Prompt[self.prompt_name].add_enum_item(self.combo_box_value)
        elif self.data_type == 'MATERIAL':
            data = bpy.data.materials[self.data_name]
            data.mv.PromptPage.COL_Prompt[self.prompt_name].add_enum_item(self.combo_box_value)
        elif self.data_type == 'WORLD':
            data = bpy.data.worlds[self.data_name]
            data.mv.PromptPage.COL_Prompt[self.prompt_name].add_enum_item(self.combo_box_value)
        return {'FINISHED'}

    def invoke(self,context,event):
        self.combo_box_value = "Option"
        if self.data_type == 'OBJECT':
            data = bpy.data.objects[self.data_name]
            Counter = 1
            while self.combo_box_value + " " + str(Counter) in data.mv.PromptPage.COL_Prompt[self.prompt_name].COL_EnumItem:
                Counter += 1
            self.combo_box_value = self.combo_box_value + " " + str(Counter)
            
        elif self.data_type == 'SCENE':
            data = bpy.data.scenes[self.data_name]
            Counter = 1
            while self.combo_box_value + " " + str(Counter) in data.mv.PromptPage.COL_Prompt[self.prompt_name].COL_EnumItem:
                Counter += 1
            self.combo_box_value = self.combo_box_value + " " + str(Counter)
            
        elif self.data_type == 'MATERIAL':
            data = bpy.data.materials[self.data_name]
            Counter = 1
            while self.combo_box_value + " " + str(Counter) in data.mv.PromptPage.COL_Prompt[self.prompt_name].COL_EnumItem:
                Counter += 1
            self.combo_box_value = self.combo_box_value + " " + str(Counter)   
            
        elif self.data_type == 'WORLD':
            data = bpy.data.worlds[self.data_name]
            Counter = 1
            while self.combo_box_value + " " + str(Counter) in data.mv.PromptPage.COL_Prompt[self.prompt_name].COL_EnumItem:
                Counter += 1
            self.combo_box_value = self.combo_box_value + " " + str(Counter)   
            
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)

    def draw(self, context):
        layout = self.layout
        layout.prop(self,"combo_box_value")
        
class OPS_update_formulas_w_new_prompt_type(Operator):
    bl_idname = "fd_prompts.update_formulas_w_new_prompt_type"
    bl_label = "Update Formulas with New Prompt Type"
    bl_options = {'UNDO'}

    data_name = StringProperty(name="Data Name")
    name = StringProperty(name="Old Name",default="Old Name")
    data_type = StringProperty(name="Data Type",default = 'OBJECT')
    new_type = StringProperty(name="Prompt Type")
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj = bpy.data.objects[self.data_name]   
        prompt = obj.mv.PromptPage.COL_Prompt[self.name]
        self.new_type = prompt.get_type_as_string(prompt.Type) 
        for obj1 in bpy.data.objects:
            if obj1.animation_data:
                for driver in obj1.animation_data.drivers:
                    for var in driver.driver.variables:
                        for target in var.targets:
                            if target.id == obj:
                                if target.data_path.find(self.name) != -1:
                                    new_data_path = target.data_path.split("].")[0] + "]." + self.new_type
                                    target.data_path = new_data_path
        return {'FINISHED'}        

class OPS_show_object_prompts(Operator):
    bl_idname = "fd_prompts.show_object_prompts"
    bl_label = "Show Object Prompts"
    bl_description = "This will display all of the prompts for the object"
    bl_options = {'UNDO'}
    
    object_bp_name = StringProperty(name="Object Base Point Name")
    tab_name = StringProperty(name="Tab Name")
    index = IntProperty(name="Index")
    
    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj_bp = bpy.data.objects[self.object_bp_name]
        obj_bp.mv.PromptPage.run_calculators(obj_bp)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=500)

    def draw(self, context):
        layout = self.layout
        obj_bp = bpy.data.objects[self.object_bp_name]
        obj_bp.mv.PromptPage.show_prompts(layout,obj_bp,self.index,self.tab_name)

#------REGISTER
classes = [
           OPS_add_prompt,
           OPS_add_calculation_prompt,
           OPS_delete_prompt,
           OPS_delete_main_tab,
           OPS_rename_prompt,
           OPS_rename_main_tab,
           OPS_show_prompt_properties,
           OPS_add_main_tab,
           OPS_add_combo_box_option,
           OPS_run_calculator,
           OPS_update_formulas_w_new_prompt_type,
           OPS_show_object_prompts
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
