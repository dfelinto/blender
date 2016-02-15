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
from bpy.types import Panel, Menu, Header, UIList

import fd

class FD_UL_materials(bpy.types.UIList):

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.label(text=item.name,icon='MATERIAL')

class FD_UL_vgroups(bpy.types.UIList):

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.label(text=item.name,icon='GROUP_VERTEX')

class FD_UL_prompttabs(UIList):
    
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.label(text=item.name)
        layout.prop(item,'type')

class FD_UL_combobox(UIList):
    
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.label(text=item.name)

class FD_UL_promptitems(UIList):
    
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.label(text=item.name)
        if item.Type == 'NUMBER':
            layout.label(str(item.NumberValue))
        if item.Type == 'CHECKBOX':
            layout.label(str(item.CheckBoxValue))
        if item.Type == 'QUANTITY':
            layout.label(str(item.QuantityValue))
        if item.Type == 'COMBOBOX':
            layout.label(str(item.EnumIndex))
        if item.Type == 'DISTANCE':
            layout.label(str(item.DistanceValue))
        if item.Type == 'ANGLE':
            layout.label(str(item.AngleValue))
        if item.Type == 'PERCENTAGE':
            layout.label(str(item.PercentageValue))
        if item.Type == 'PRICE':
            layout.label(str(item.PriceValue))

class FD_UL_objects(bpy.types.UIList):
    
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if item.name not in bpy.data.objects:
            layout.label(text=item.name + " *(missing)",icon='ERROR')
        else:
            obj = bpy.data.objects[item.name]
            
            if obj.type == 'MESH':
                layout.label(text=obj.mv.name_object,icon=fd.const.icon_mesh)
    
            if obj.type == 'EMPTY':
                layout.label(text=obj.mv.name_object,icon=fd.const.icon_empty)
                    
                if obj.mv.use_as_mesh_hook:
                    layout.label(text="",icon='HOOK')
                    
            if obj.type == 'CURVE':
                layout.label(text=obj.mv.name_object,icon=fd.const.icon_curve)
                
            if obj.type == 'FONT':
                layout.label(text=obj.mv.name_object,icon=fd.const.icon_font)
    
            layout.operator("fd_object.select_object_by_name",icon='MAN_TRANS',text="").object_name = item.name
            layout.operator("fd_assembly.delete_object_in_assembly",icon='X',text="",emboss=False).object_name = obj.name

classes = [
           FD_UL_materials,
           FD_UL_vgroups,
           FD_UL_prompttabs,
           FD_UL_combobox,
           FD_UL_objects,
           FD_UL_promptitems
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

