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
from bpy.types import Header, Menu, Operator, PropertyGroup
import math

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

class OPS_remove_world(Operator):# IMPLEMENT ALL DELETE FUNCTIONS 
    bl_idname = "fd_world.remove_world"
    bl_label = "Remove Group"
    bl_options = {'UNDO'}
    
    world_name = StringProperty(name="World Name")

    def execute(self, context):
        world = bpy.data.worlds[self.world_name]
        if world.users == 0:
            bpy.data.worlds.remove(world)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=300)

    def draw(self, context):
        world = bpy.data.worlds[self.world_name]
        layout = self.layout
        if world.users == 0:
            layout.label("Are you sure you want to remove " + world.name)
        else:
            layout.label("Cannot remove datablock with " + str(world.users) + " users.")

class OPS_clear_unused_worlds_from_file(Operator):# IMPLEMENT ALL DELETE FUNCTIONS 
    bl_idname = "fd_world.clear_unused_worlds_from_file"
    bl_label = "Clear Unused Groups From File"
    bl_options = {'UNDO'}
    
    def execute(self, context):
        for world in bpy.data.worlds:
            if world.users == 0:
                bpy.data.worlds.remove(world)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.label("Are you sure you want to remove all unused worlds from the file?",icon='QUESTION')

class OPS_show_world_options(Operator):# IMPLEMENT ALL DELETE FUNCTIONS 
    bl_idname = "fd_world.show_world_options"
    bl_label = "Show World Options"
    bl_options = {'UNDO'}
    
    world_name = StringProperty(name="World Name")
    
    def execute(self, context):
        return {'FINISHED'}
    
    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=300)
    
    def draw(self, context):
        layout = self.layout
        world = bpy.data.worlds[self.world_name]
        propbox = layout.box()
        propbox.label("Active World Properties:")
        propbox.prop(world,'name')
        if world and world.node_tree:
            for node in world.node_tree.nodes:
                if node.type == 'BACKGROUND':
                    propbox.prop(node.inputs[1],'default_value',text="Strength")
                if node.type == 'MAPPING':
                    propbox.prop(node,'rotation')
                    
#------REGISTER
classes = [
           OPS_remove_world,
           OPS_clear_unused_worlds_from_file,
           OPS_show_world_options
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
