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


class NewGeometryNodeTree(bpy.types.Operator):
    """Create a new geometry node tree"""

    bl_idname = "node.new_geometry_node_tree"
    bl_label = "New Geometry Node Tree"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.area.type == 'NODE_EDITOR' and context.space_data.tree_type == 'GeometryNodeTree'

    def execute(self, context):
        group = bpy.data.node_groups.new("Geometry Node Group", 'GeometryNodeTree')
        group.inputs.new('NodeSocketGeometry', "Geometry")
        group.outputs.new('NodeSocketGeometry', "Geometry")
        input_node = group.nodes.new('NodeGroupInput')
        output_node = group.nodes.new('NodeGroupOutput')

        input_node.location.x = -200 - input_node.width
        output_node.location.x = 200

        group.links.new(output_node.inputs[0], input_node.outputs[0])

        context.space_data.node_tree = group
        return {'FINISHED'}


classes = (
    NewGeometryNodeTree,
)
