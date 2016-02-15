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
from bpy.types import Operator

from bpy.props import (StringProperty,
                       BoolProperty,
                       IntProperty,
                       FloatProperty,
                       FloatVectorProperty,
                       BoolVectorProperty,
                       PointerProperty,
                       EnumProperty,
                       CollectionProperty)

import fd

class OPS_make_group_from_selection(Operator):
    bl_idname = "fd_group.make_group_from_selection"
    bl_label = "Make Group From Selection"
    bl_description = "This will create a group from the selected objects"
    bl_options = {'UNDO'}

    group_name = StringProperty(name="Group Name",default = "New Group")

    @classmethod
    def poll(cls, context):
        if len(context.selected_objects) > 1:
            return True
        else:
            return False

    def execute(self, context):
        for obj in context.selected_objects:
            obj.hide = False
            obj.hide_select = False
            obj.select = True
        bpy.ops.group.create(name=self.group_name)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self,"group_name")

class OPS_remove_group(Operator):
    bl_idname = "fd_group.remove_group"
    bl_label = "Remove Group"
    bl_options = {'UNDO'}
    
    group_name = StringProperty(name="Group Name")

    def execute(self, context):
        grp = bpy.data.groups[self.group_name]
        bpy.data.groups.remove(grp)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=300)

    def draw(self, context):
        grp = bpy.data.groups[self.group_name]
        layout = self.layout
        layout.label("Are you sure you want to remove " + grp.name)

class OPS_clear_all_groups_from_file(Operator):
    bl_idname = "fd_group.clear_all_groups_from_file"
    bl_label = "Clear All Groups From File"
    bl_options = {'UNDO'}
    
    def execute(self, context):
        for grp in bpy.data.groups:
            bpy.data.groups.remove(grp)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=300)

    def draw(self, context):
        layout = self.layout
        layout.label("Are you sure you want to remove all groups from the file?")

class OPS_show_group_options(Operator):
    bl_idname = "fd_group.show_group_options"
    bl_label = "Show Group Options"
    bl_options = {'UNDO'}
    
    group_name = StringProperty(name="Group Name")
    
    def execute(self, context):
        for grp in bpy.data.groups:
            bpy.data.groups.remove(grp)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=300)

    def draw(self, context):
        layout = self.layout
        grp = bpy.data.groups[self.group_name]
        layout.prop(grp,'name',icon='GROUP')
        layout.operator('fd_group.remove_group',text='Remove Group').group_name = grp.name

#------REGISTER
classes = [
           OPS_make_group_from_selection,
           OPS_remove_group,
           OPS_clear_all_groups_from_file,
           OPS_show_group_options
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
