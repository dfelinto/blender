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

class OPS_add_wall(Operator):
    bl_idname = "fd_assembly.add_wall"
    bl_label = "Draw Wall"
    bl_description = "Draw a Wall"
#     bl_options = {'UNDO'} This is pushed manually at the end of execute
     
    add_to_selected = BoolProperty(name="Add New Wall to Selected",default=True)
     
    direction = EnumProperty(name="Placement on Wall",
                             items=[('LEFT',"Left",""),
                                    ('STRAIGHT',"Straight",""),
                                    ('RIGHT',"Right","")],
                             default='STRAIGHT')
    
    wall_length = FloatProperty(name="Wall Length",default=fd.inches(120),unit='LENGTH')
    wall_height = FloatProperty(name="Wall Height",default=fd.inches(108),unit='LENGTH')
    
    rotation = FloatProperty(name="Rotation")
    group_wall = None
    selected_wall = None
    zoom_all = False
    delete_wall = True
    
    @classmethod
    def poll(cls, context):
        return True
     
    def check(self, context):
        self.update_wall(context)
        return True
     
    def __del__(self):
        if self.delete_wall: # Only Delete Wall if user didn't click OK
            fd.delete_assembly_from_bp(self.group_wall.obj_bp)
            
    def invoke(self,context,event):
        wm = context.window_manager
        if context.active_object:
            obj_wall_bp = fd.get_wall_bp(context.active_object)
            if obj_wall_bp:
                self.selected_wall = fd.Wall(obj_wall_bp)
                self.rotation = math.degrees(obj_wall_bp.rotation_euler.z)
            else:
                self.direction = 'STRAIGHT'
                self.rotation = 0
        else:
            self.direction = 'STRAIGHT'
            self.rotation = 0
        
        self.create_wall(context)
        self.update_wall(context)
        return wm.invoke_props_dialog(self, width=400)
         
    def create_wall(self,context):
        self.group_wall = fd.Wall()
        self.group_wall.create_wall()
        obj_mesh = self.group_wall.build_wall_mesh()
        self.group_wall.obj_bp.location = (0,0,0)
        obj_mesh.draw_type = 'WIRE'

    def update_wall(self,context):
        wm = context.window_manager
        if self.direction == 'LEFT':
            wm.mv.wall_rotation = self.rotation + 90
        if self.direction == 'STRAIGHT':
            wm.mv.wall_rotation = self.rotation
        if self.direction == 'RIGHT':
            wm.mv.wall_rotation = self.rotation + -90
        self.group_wall.obj_bp.rotation_euler.z = math.radians(wm.mv.wall_rotation)
        self.group_wall.obj_x.location.x = self.wall_length
        self.group_wall.obj_y.location.y = fd.inches(1)
        self.group_wall.obj_z.location.z = self.wall_height

        if self.found_wall(context) == False or self.add_to_selected == False:
            self.group_wall.obj_bp.location = context.scene.cursor_location
        else:
            if self.selected_wall:
                self.group_wall.obj_bp.location.x = self.selected_wall.obj_x.matrix_world[0][3]
                self.group_wall.obj_bp.location.y = self.selected_wall.obj_x.matrix_world[1][3]
            else:
                self.group_wall.obj_bp.location = context.scene.cursor_location
        self.group_wall.refresh_hook_modifiers()
        self.group_wall.obj_x.hide = True
        self.group_wall.obj_y.hide = True
        self.group_wall.obj_z.hide = True
        
    def draw(self,context):
        layout = self.layout
        box = layout.box()
        col = box.column(align=False)
        
        row = col.row(align=True)
        row.prop_enum(self, "direction", 'LEFT', icon='TRIA_LEFT', text="Left") 
        row.prop_enum(self, "direction", 'STRAIGHT', icon='TRIA_UP', text="Straight") 
        row.prop_enum(self, "direction", 'RIGHT', icon='TRIA_RIGHT', text="Right")   
        
        row = col.row()
        row.label("Wall Length:")
        row.prop(self,"wall_length",text="")
        
        row = col.row()
        row.label("Wall Height:")
        row.prop(self,"wall_height",text="")

        if self.found_wall(context):
            row = col.row()
            row.prop(self,"add_to_selected",text="Add to Selected Wall")

    def found_wall(self,context):
        for obj in context.visible_objects:
            if obj.mv.type == 'BPWALL':
                return True
        return False
            
    def execute(self, context):
        self.delete_wall = False
        for child in self.group_wall.obj_bp.children:
            if child.type == 'MESH':
                child.draw_type = 'TEXTURED'
                
        if self.found_wall(context) == False or self.add_to_selected == False:
            self.group_wall.obj_bp.location = context.scene.cursor_location
        else:
            if self.selected_wall:
                fd.connect_objects_location(self.selected_wall.obj_x,self.group_wall.obj_bp)
            
        if self.zoom_all:
            bpy.ops.view3d.view_all(center=True)
             
        # THIS MUST BE PUSHED MANUALLY OTHERWISE WALL MESHES 
        # BECOME WARPED WHEN USING bl_options = {'UNDO'}
        bpy.ops.ed.undo_push()
        return {'FINISHED'}

class OPS_add_assembly(Operator):
    bl_idname = "fd_assembly.add_assembly"
    bl_label = "Empty Assembly"
    bl_description = "This operator adds a new empty assembly to the scene."
    bl_options = {'UNDO'}
    
    assembly_size = FloatVectorProperty(name="Group Size",
                                        default=(fd.inches(24),fd.inches(24),fd.inches(24)))
    
    @classmethod
    def poll(cls, context):
        return True
    
    def execute(self, context):
        group = fd.Assembly()
        group.create_assembly()
        group.obj_x.location.x = self.assembly_size[0]
        group.obj_y.location.y = self.assembly_size[1]
        group.obj_z.location.z = self.assembly_size[2]
        group.build_cage()
        bpy.ops.object.select_all(action='DESELECT')
        group.obj_bp.select = True
        context.scene.objects.active = group.obj_bp
        return {'FINISHED'}

class OPS_add_mesh_to_assembly(Operator):
    """ Since this looks to the context this should only be called from the ui.
    """
    bl_idname = "fd_assembly.add_mesh_to_assembly"
    bl_label = "Add Mesh To Assembly"
    bl_options = {'UNDO'}
    
    mesh_name = StringProperty(name="Mesh Name",default="New Mesh")

    @classmethod
    def poll(cls, context):
        if context.active_object:
            obj_bp = fd.get_assembly_bp(context.active_object)
            if obj_bp:
                return True
        return False

    def execute(self, context):
        obj_bp = fd.get_assembly_bp(context.active_object)
        group = fd.Assembly(obj_bp)
        obj_bp = group.obj_bp
        dim_x = group.obj_x.location.x
        dim_y = group.obj_y.location.y
        dim_z = group.obj_z.location.z

        obj_mesh = fd.create_cube_mesh(self.mesh_name,(dim_x,dim_y,dim_z))
                
        if obj_mesh:
            obj_mesh.mv.name_object = self.mesh_name
            context.scene.objects.active = obj_mesh
            bpy.ops.object.editmode_toggle()
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.mesh.normals_make_consistent(inside=False)
            bpy.ops.object.editmode_toggle()
            if obj_bp:
                obj_mesh.parent = obj_bp

            fd.update_vector_groups(obj_bp)
            bpy.ops.fd_assembly.load_active_assembly_objects(object_name=obj_bp.name)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "mesh_name")

class OPS_add_empty_to_assembly(Operator):
    """ Since this looks to the context this should only be called from the ui.
    """
    bl_idname = "fd_assembly.add_empty_to_assembly"
    bl_label = "Add Empty To Assembly"
    bl_options = {'UNDO'}
    
    use_as_mesh_hook = BoolProperty(name="Use As Mesh Hook",default=False)
    empty_name = StringProperty(name="Empty Name",default="New Empty")

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj_bp = fd.get_assembly_bp(context.active_object)

        #NOTE: Since Mesh hooks are maintained by object name
        #      You cannot have two emptyhooks with the same name.       
        for child in obj_bp.children:
            if child.type == 'EMPTY' and self.use_as_mesh_hook and child.mv.use_as_mesh_hook and child.mv.name_object == self.empty_name:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',Message="A hook with that name already exists.")
                return {'CANCELLED'}
            
        #NOTE: Since Mesh hooks are maintained by object name
        #      These names are reserved the the visible prompts of the group
        if self.use_as_mesh_hook:
            if self.empty_name in {'Dimension X','Dimension Y','Dimension Z'}:
                bpy.ops.fd_general.error('INVOKE_DEFAULT',Message="That hook name are reserved for visible prompts")
                return {'CANCELLED'}
        
        bpy.ops.object.empty_add()
        obj_empty = context.active_object

        if obj_empty:
            obj_empty.empty_draw_size = fd.inches(1)
            obj_empty.mv.name_object = self.empty_name
            obj_empty.mv.use_as_mesh_hook = self.use_as_mesh_hook
            if obj_bp:
                obj_empty.parent = obj_bp
            
            context.scene.objects.active = obj_empty
            fd.update_vector_groups(obj_bp)
            bpy.ops.fd_assembly.load_active_assembly_objects(object_name=obj_bp.name)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "empty_name")
        layout.prop(self, "use_as_mesh_hook")
        
class OPS_add_curve_to_assembly(Operator):
    """ Since this looks to the context this should only be called from the ui.
    """
    bl_idname = "fd_assembly.add_curve_to_assembly"
    bl_label = "Add Curve To Assembly"
    bl_options = {'UNDO'}
    
    use_selected = BoolProperty(name="Use Selected",default=False)
    curve_name = StringProperty(name="Curve Name",default="New Curve")

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj_bp = fd.get_assembly_bp(context.active_object)
        group = fd.Assembly(obj_bp)
        dim_x = group.obj_x.location.x

        bpy.ops.curve.primitive_bezier_curve_add(enter_editmode=False)
        obj_curve = context.active_object
        obj_curve.data.show_handles = False
        
        obj_curve.data.splines[0].bezier_points[0].co = (0,0,0)
        obj_curve.data.splines[0].bezier_points[1].co = (dim_x,0,0)
        
        bpy.ops.object.editmode_toggle()
        bpy.ops.curve.select_all(action='SELECT')
        bpy.ops.curve.handle_type_set(type='VECTOR')
        bpy.ops.object.editmode_toggle()
        obj_curve.data.dimensions = '2D'
        
        if obj_curve:
            obj_curve.mv.name_object = self.curve_name
            obj_curve.parent = obj_bp
            bpy.ops.fd_assembly.load_active_assembly_objects(object_name=obj_bp.name)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "curve_name")

class OPS_add_text_to_assembly(Operator):
    """ Since this looks to the context this should only be called from the ui.
    """
    bl_idname = "fd_assembly.add_text_to_assembly"
    bl_label = "Add Text To Assembly"
    bl_options = {'UNDO'}

    text_name = StringProperty(name="Text Name",default="New Text")

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj_bp = fd.get_assembly_bp(context.active_object)

        bpy.ops.object.text_add()
        obj_text = context.active_object

        if obj_text:
            obj_text.mv.name_object = self.text_name

            if obj_bp:
                obj_text.parent = obj_bp
            
            context.scene.objects.active = obj_text
            bpy.ops.fd_group.load_active_group_objects(object_name=obj_bp.name)     
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "text_name")
    
class OPS_make_group_from_selected_assembly(Operator):
    bl_idname = "fd_assembly.make_group_from_selected_assembly"
    bl_label = "Make Group From Selected Assembly"
    bl_description = "This will create a group from the selected assembly"
    bl_options = {'UNDO'}
    
    assembly_name = StringProperty(name="Group Name",default = "New Group")
    
    @classmethod
    def poll(cls, context):
        obj_bp = fd.get_parent_assembly_bp(context.object)
        if obj_bp:
            return True
        else:
            return False

    def execute(self, context):
        obj_bp = fd.get_parent_assembly_bp(context.object)
        if obj_bp:
            list_obj = fd.get_child_objects(obj_bp)
            for obj in list_obj:
                obj.hide = False
                obj.hide_select = False
                obj.select = True
            bpy.ops.group.create(name=self.assembly_name)
            
            obj_bp.location = obj_bp.location
        return {'FINISHED'}

    def invoke(self,context,event):
        obj_bp = fd.get_parent_assembly_bp(context.object)
        if obj_bp:
            self.assembly_name = obj_bp.mv.name_object
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self,"assembly_name")

class OPS_select_selected_assembly_base_point(Operator):
    bl_idname = "fd_assembly.select_selected_assemby_base_point"
    bl_label = "Select Assembly Base Point"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        obj_bp = fd.get_parent_assembly_bp(obj)
        if obj_bp:
            return True
        else:
            return False

    def execute(self, context):
        obj = context.active_object
        obj_bp = fd.get_parent_assembly_bp(obj)
        if obj_bp:
            bpy.ops.object.select_all(action='DESELECT')
            obj_bp.hide = False
            obj_bp.hide_select = False
            obj_bp.select = True
            context.scene.objects.active = obj_bp
        return {'FINISHED'}

class OPS_delete_object_in_assembly(Operator):
    bl_idname = "fd_assembly.delete_object_in_assembly"
    bl_label = "Delete Object in Assembly"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name="Object Name")
    
    @classmethod
    def poll(cls, context):
        return True
    
    def execute(self, context):
        obj = bpy.data.objects[self.object_name]
        obj_bp = obj.parent
        bpy.ops.object.select_all(action='DESELECT')
        obj.select = True
        context.scene.objects.active = obj
        bpy.ops.object.delete()
        bpy.ops.fd_assembly.load_active_assembly_objects(object_name=obj_bp.name)
        obj_bp.select = True
        context.scene.objects.active = obj_bp #Set Base Point as active object so panel doesn't disappear
        return {'FINISHED'}
    
class OPS_load_active_assembly_objects(Operator):
    bl_idname = "fd_assembly.load_active_assembly_objects"
    bl_label = "Load Active Assembly Objects"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name="Object Name")
    
    @classmethod
    def poll(cls, context):
        return True
    
    def execute(self, context):
        obj = bpy.data.objects[self.object_name]
        scene = context.scene.mv
        
        for current_obj in scene.children_objects:
            scene.children_objects.remove(0)
        
        group = fd.Assembly(obj)
        group.set_object_names()
        
        scene.active_object_name = obj.name
        for child in obj.children:
            if child.mv.type not in {'VPDIMX','VPDIMY','VPDIMZ','CAGE'}:
                list_obj = scene.children_objects.add()
                list_obj.name = child.name
            
        return {'FINISHED'}
    
class OPS_rename_assembly(Operator):
    bl_idname = "fd_assembly.rename_assembly"
    bl_label = "Rename Assembly"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name="Object Name")
    new_name = StringProperty(name="New Name",default="")

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        obj_bp = bpy.data.objects[self.object_name]
        obj_bp.mv.name_object = self.new_name
        group = fd.Assembly(obj_bp)
        group.set_object_names()
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        obj_bp = bpy.data.objects[self.object_name]
        self.new_name = obj_bp.mv.name_object
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "new_name")

class OPS_delete_selected_assembly(Operator):
    bl_idname = "fd_assembly.delete_selected_assembly"
    bl_label = "Delete Selected Assembly"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name="Object Name")
    
    @classmethod
    def poll(cls, context):
        obj = context.active_object
        obj_bp = fd.get_parent_assembly_bp(obj)
        if obj_bp:
            return True
        else:
            return False
    
    def execute(self, context):
        obj = context.object
        obj_bp = fd.get_parent_assembly_bp(obj)
        obj_list = []
        obj_list = fd.get_child_objects(obj_bp,obj_list)
        fd.delete_obj_list(obj_list)
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=300)

    def draw(self, context):
        obj = context.object
        obj_bp = fd.get_parent_assembly_bp(obj)
        layout = self.layout
        layout.label("Group Name: " + obj_bp.mv.name_object)

class OPS_copy_selected_assembly(Operator):
    bl_idname = "fd_assembly.copy_selected_assembly"
    bl_label = "Copy Selected Assembly"
    bl_options = {'UNDO'}
    
    @classmethod
    def poll(cls, context):
        obj = context.active_object
        obj_bp = fd.get_parent_assembly_bp(obj)
        if obj_bp:
            return True
        else:
            return False
    
    def execute(self, context):
        obj_bools = []
        obj = context.object
        obj_bp = fd.get_parent_assembly_bp(obj)
        if obj_bp:
            obj_list = []
            obj_list = fd.get_child_objects(obj_bp,obj_list)
            bpy.ops.object.select_all(action='DESELECT')
            for obj in obj_list:
                obj.hide = False
                obj.select = True
            
            bpy.ops.object.duplicate()
            
            for obj in context.selected_objects:
                if obj.type == 'EMPTY':
                    obj.hide = True
                #COLLECT BOOLEAN OBJECTS FROM GROUP
                if obj.mv.use_as_bool_obj:
                    obj_bools.append(obj)
                obj.location = obj.location
            
            for obj in obj_list:
                if obj.type == 'EMPTY':
                    obj.hide = True
                obj.location = obj.location
            bpy.ops.object.select_all(action='DESELECT')
            
            #ASSIGN BOOLEAN MODIFIERS TO WALL
            if obj_bp.parent:
                if obj_bp.parent.mv.type == 'BPWALL':
                    wall = fd.Wall(obj_bp.parent)
                    mesh = wall.get_wall_mesh()
                    for obj_bool in obj_bools:
                        mod = mesh.modifiers.new(obj_bool.name,'BOOLEAN')
                        mod.object = obj_bool
                        mod.operation = 'DIFFERENCE'
            
            obj_bp.select = True
            context.scene.objects.active = obj_bp
            
        return {'FINISHED'}

class OPS_connect_mesh_to_hooks_in_assembly(Operator):
    bl_idname = "fd_assembly.connect_meshes_to_hooks_in_assembly"
    bl_label = "Connect Mesh to Hooks In Assembly"
    bl_options = {'UNDO'}
    
    object_name = StringProperty(name="Object Name")
    
    @classmethod
    def poll(cls, context):
        return True
    
    def execute(self, context):
        obj = bpy.data.objects[self.object_name]
        group_bp = fd.get_assembly_bp(obj)
        wall_bp = fd.get_wall_bp(obj)
        if group_bp:
            group = fd.Assembly(group_bp)
        elif wall_bp:
            group = fd.Wall(wall_bp)
        hooklist = []
        for child in group.obj_bp.children:
            if child.type == 'EMPTY'  and child.mv.use_as_mesh_hook:
                hooklist.append(child)
        
        fd.apply_hook_modifiers(obj)
        for vgroup in obj.vertex_groups:
            if vgroup.name == 'X Dimension':
                fd.hook_vertex_group_to_object(obj,'X Dimension',group.obj_x)
            elif vgroup.name == 'Y Dimension':
                fd.hook_vertex_group_to_object(obj,'Y Dimension',group.obj_y)
            elif vgroup.name == 'Z Dimension':
                fd.hook_vertex_group_to_object(obj,'Z Dimension',group.obj_z)
            else:
                for hook in hooklist:
                    if hook.mv.name_object == vgroup.name:
                        fd.hook_vertex_group_to_object(obj,vgroup.name,hook)
                
        return {'FINISHED'}

# class OPS_edit_dimension_global_properties(Operator):
#     bl_idname = "fd_group.edit_dimension_global_properties"
#     bl_label = "Dimension Global Properties"
#     bl_options = {'UNDO'}
#     
#     obj_dimensions = []
#     
#     dim_thickness = FloatProperty(name="Dimension Thickness")
#     text_size = FloatProperty(name="Text Size") 
#     dim_arrow = EnumProperty(items=fd.enums.enum_visual_dimension_arrows,
#                              name="Arrow Type",
#                              default='Arrow1')
#     #TODO
#     measurement_precision = IntProperty(name="Measurement Precision")
# 
#     @classmethod
#     def poll(cls, context):
#         for obj in context.visible_objects:
#             if obj.Dimension:
#                 return True    
#             
#     def get_dimensions(self):
#         self.obj_dimensions = []
#         for obj in bpy.context.visible_objects:
#             if obj.Dimension:
#                 dim_info = (obj, 
#                             obj.Dimension_startlocation, 
#                             obj.Dimension_endlocation, 
#                             obj.Dimension_XZType,
#                             obj.Dimension_length,
#                             obj.Dimension_Type)
# 
#                 self.obj_dimensions.append(dim_info)
#                 
#                 self.text_size = obj.Dimension_textsize
#                 self.dim_thickness = obj.Dimension_depth  
#                 self.dim_arrow = obj.Dimension_arrow
#     
#     def invoke(self,context,event):
#         self.get_dimensions()        
#         wm = context.window_manager
#         return wm.invoke_props_dialog(self, width=400)        
#     
#     def draw(self,context):
#         layout = self.layout
#         box = layout.box()
#         box.prop(self, 'text_size')
#         box.prop(self, "dim_thickness")
#         box.prop(self, "dim_arrow")
# 
#     def execute(self, context):
#         objs_to_del = []
#         for i, obj in enumerate(self.obj_dimensions):
#             objs_to_del.append(obj[0])
#             objs_to_del.append(obj[0].children[0])
#             bpy.ops.curve.dimension(Dimension_startlocation = obj[1],
#                                     Dimension_endlocation = obj[2],
#                                     Dimension_XZType = obj[3],
#                                     Dimension_length = obj[4],
#                                     Dimension_Type = obj[5],
#                                     Dimension_width_or_location = 'location',
#                                     Dimension_liberty = '2D',
#                                     Dimension_rotation = 0,
#                                     Dimension_XYZType = 'FRONT',
#                                     Dimension_depth = self.dim_thickness,
#                                     Dimension_textsize = self.text_size,
#                                     Dimension_arrow = self.dim_arrow,     
#                                     Dimension_arrowdepth = 0.5,
#                                     Dimension_arrowlength = 1.5)                     
#         fd_utils.delete_obj_list(objs_to_del) 
#         self.get_dimensions()
# 
#         return {'FINISHED'}    
#     
# class OPS_change_visual_dimension(Operator):
#     bl_idname = "fd_group.change_visual_dimension"
#     bl_label = "Change Visual Dimension"
#     bl_options = {'UNDO'}
#     
#     dim_len = FloatProperty(name="Leader Length")
# 
#     @classmethod
#     def poll(cls, context):
#         # TODO
#         return True
# 
#     def check(self,context):    
#         obj = context.object
#         bpy.ops.curve.dimension(Dimension_Change = True,
#                                 Dimension_Delete = obj.name,
#                                 Dimension_width_or_location = obj.Dimension_width_or_location,
#                                 Dimension_startlocation = obj.location,
#                                 Dimension_endlocation = obj.Dimension_endlocation,
#                                 Dimension_endanglelocation = obj.Dimension_endanglelocation,
#                                 Dimension_liberty = obj.Dimension_liberty,
#                                 Dimension_Type = obj.Dimension_Type,
#                                 Dimension_XYZType = obj.Dimension_XYZType,
#                                 Dimension_XYType = obj.Dimension_XYType,
#                                 Dimension_XZType = obj.Dimension_XZType,
#                                 Dimension_YZType = obj.Dimension_YZType,
#                                 Dimension_resolution = obj.Dimension_resolution,
#                                 Dimension_width = obj.Dimension_width,
#                                 Dimension_length = self.dim_len,
#                                 Dimension_dsize = obj.Dimension_dsize,
#                                 Dimension_depth = obj.Dimension_depth,
#                                 Dimension_depth_from_center = obj.Dimension_depth_from_center,
#                                 Dimension_angle = obj.Dimension_angle,
#                                 Dimension_rotation = obj.Dimension_rotation,
#                                 Dimension_textsize = obj.Dimension_textsize,
#                                 Dimension_textdepth = obj.Dimension_textdepth,
#                                 Dimension_textround = obj.Dimension_textround,
#                                 Dimension_matname = obj.Dimension_matname,
#                                 Dimension_note = obj.Dimension_note,
#                                 Dimension_arrow = obj.Dimension_arrow,
#                                 Dimension_arrowdepth = obj.Dimension_arrowdepth,
#                                 Dimension_arrowlength = obj.Dimension_arrowlength)          
#          
#         return True
# 
#     def invoke(self,context,event):
#         obj = context.object
#         self.dim_len = (obj.Dimension_length) 
#         
#         wm = context.window_manager
#         return wm.invoke_props_dialog(self, width=400)        
#     
#     def draw(self,context):
#         layout = self.layout
#         layout.prop(self, "dim_len")
#         
#     def execute(self,context):
#         return {'FINISHED'}
#         
# class OPS_add_visual_dimensions(Operator):
#     bl_idname = "fd_group.add_visual_dimensions"
#     bl_label = "Add Visual Dimensions"
#     bl_options = {'UNDO'}
#     
#     group_name = StringProperty(name="Group Name")
#     dim_to_draw = EnumProperty(name="Visual Dimension", items=fd.enums.enum_visual_dimensions)
#     placement_AB = EnumProperty(name="Visual Dimension Left/Right Placement", items=fd.enums.enum_ab_visual_dimension_placement)
#     placement_LR = EnumProperty(name="Visual Dimension Above/Below Placement", items=fd.enums.enum_lr_visual_dimension_placement)
# 
#     @classmethod
#     def poll(cls, context):
# #         dm = context.scene.mv.dm
# #         grp = dm.get_product_group(context.active_object)
# #         if grp:
#         return True
#         
#     def check(self, context):
#         return True
#     
#     def execute(self, context):
#         obj_bp_col = []
#         for obj in bpy.context.selected_objects:
#             obj_bp = fd.get_product_bp(obj)
#             if obj_bp:
#                 if obj_bp not in obj_bp_col:
#                     obj_bp_col.append(obj_bp)
#             else:
#                 if not obj_bp:
#                     obj_bp = fd.get_wall_bp(obj)
#                     if obj_bp not in obj_bp_col:
#                         obj_bp_col.append(obj_bp)
#                 
#         for obj_bp in obj_bp_col:
#             group = fd.Assembly(obj_bp)
#             obj_bp_loc = group.obj_bp.location
#             obj_xdim_loc = group.obj_x.location
#             obj_ydim_loc = group.obj_y.location
#             obj_zdim_loc = group.obj_z.location
#             
#             if self.dim_to_draw == 'X':
#                 dim_name = "DIMENSION_X." + obj_bp.name.split(".", 1)[1]
#                 if self.placement_AB == 'ABOVE':
#                     if obj_zdim_loc[2] < 0:
#                         start_vertex = (obj_bp_loc[0], obj_ydim_loc[1], obj_bp_loc[2])
#                         end_vertex = ((obj_bp_loc[0] + obj_xdim_loc[0]), obj_xdim_loc[1], obj_bp_loc[2])
#                     else:
#                         start_vertex = (obj_bp_loc[0], obj_ydim_loc[1], obj_zdim_loc[2])
#                         end_vertex = ((obj_bp_loc[0] + obj_xdim_loc[0]), obj_xdim_loc[1], obj_zdim_loc[2])                        
#                     dim_len = 10.0
#                     xz = 'X'
#                     
#                 if self.placement_AB == 'BELOW':
#                     if obj_zdim_loc[2] < 0:
#                         start_vertex = (obj_bp_loc[0], obj_ydim_loc[1], (obj_bp_loc[2]+obj_zdim_loc[2]))
#                         end_vertex = ((obj_bp_loc[0] + obj_xdim_loc[0]), obj_xdim_loc[1], (obj_bp_loc[2]+obj_zdim_loc[2]))
#                     else:
#                         start_vertex = (obj_bp_loc[0], obj_ydim_loc[1], 0)
#                         end_vertex = ((obj_bp_loc[0] + obj_xdim_loc[0]), obj_xdim_loc[1], 0)                        
#                     dim_len = -10.0
#                     xz = 'X'
#                 
#             if self.dim_to_draw == 'Y':
#                 dim_name = "DIMENSION_Y." + obj_bp.name.split(".", 1)[1]
# 
#             if self.dim_to_draw == 'Z':
#                 dim_name = "DIMENSION_Z." + obj_bp.name.split(".", 1)[1]
#                 if self.placement_LR == 'LEFT':
#                     if obj_zdim_loc[2] < 0:
#                         start_vertex = (obj_bp_loc[0], obj_ydim_loc[1], obj_bp_loc[2])
#                         end_vertex = (obj_bp_loc[0], obj_xdim_loc[1], (obj_bp_loc[2] + obj_zdim_loc[2]))                        
#                     else:
#                         start_vertex = (obj_bp_loc[0], obj_ydim_loc[1], 0)
#                         end_vertex = (obj_bp_loc[0], obj_xdim_loc[1], obj_zdim_loc[2])
#                     dim_len = 10.0
#                     xz = 'Z'
#                     
#                 if self.placement_LR == 'RIGHT':
#                     if obj_zdim_loc[2] < 0:
#                         start_vertex = ((obj_bp_loc[0] + obj_xdim_loc[0]), obj_ydim_loc[1], obj_bp_loc[2])
#                         end_vertex = ((obj_bp_loc[0] + obj_xdim_loc[0]), obj_xdim_loc[1], (obj_bp_loc[2] + obj_zdim_loc[2]))
#                     else:
#                         start_vertex = ((obj_bp_loc[0] + obj_xdim_loc[0]), obj_ydim_loc[1], 0)
#                         end_vertex = ((obj_bp_loc[0] + obj_xdim_loc[0]), obj_xdim_loc[1], obj_zdim_loc[2])
#                     dim_len = -10.0    
#                     xz = 'Z'
#     
#             bpy.ops.curve.dimension(Dimension_Name = dim_name,
#                                     Dimension_startlocation = start_vertex,
#                                     Dimension_endlocation = end_vertex,
#                                     Dimension_Change = False,
#                                     Dimension_Type = 'Linear-1',
#                                     Dimension_width_or_location = 'location',
#                                     Dimension_liberty = '2D',
#                                     Dimension_rotation = 0,
#                                     Dimension_XYZType = 'FRONT',
#                                     Dimension_length = dim_len,
#                                     Dimension_XZType = xz,
#                                     Dimension_depth = 0.3,
#                                     Dimension_textsize = 2.5,
#                                     Dimension_textround = 4,
#                                     Dimension_arrowdepth = 0.5,
#                                     Dimension_arrowlength = 1.5)          
# 
#         return {'FINISHED'}
# 
#     def invoke(self,context,event):
#         wm = context.window_manager
#         return wm.invoke_props_dialog(self, width=400)
# 
#     def draw(self, context):
#         layout = self.layout
#         layout.prop(self,"dim_to_draw",text="Draw")
#         if self.dim_to_draw == 'X':
#             layout.prop(self,"placement_AB",text="Placement")  
#         if self.dim_to_draw == 'Z':
#             layout.prop(self,"placement_LR",text="Placement")
# 
# class OPS_calculate_dimension_notations(Operator):
#     bl_idname = "fd_group.calculate_dimension_notations"
#     bl_label = "Calculate Dimension Notations"
#     bl_options = {'UNDO'}
#     
#     @classmethod
#     def poll(cls, context):
#         return True
# 
#     def execute(self, context):
#         for grp in bpy.data.groups:
#             if grp.mv.type == 'PRODUCT':
#                 grp.mv.calc_dimension_notations()
#         return {'FINISHED'}
#     
# class OPS_show_all_dimension_notations(Operator):
#     bl_idname = "fd_group.show_all_dimension_notations"
#     bl_label = "show All Dimension Notations"
#     bl_options = {'UNDO'}
#     
#     @classmethod
#     def poll(cls, context):
#         return True
# 
#     def execute(self, context):
#         for grp in bpy.data.groups:
#             if grp.mv.type == 'PRODUCT':
#                 prod_bp = grp.mv.get_bp()
#                 prod_bp.mv.PromptPage.COL_Prompt["Show Dimensions"].EnumIndex = 1
#         return {'FINISHED'}

#------REGISTER
classes = [
           OPS_add_wall,
           OPS_add_assembly,
           OPS_add_mesh_to_assembly,
           OPS_add_empty_to_assembly,
           OPS_add_curve_to_assembly,
           OPS_add_text_to_assembly,
           OPS_make_group_from_selected_assembly,
           OPS_select_selected_assembly_base_point,
           OPS_delete_object_in_assembly,
           OPS_load_active_assembly_objects,
           OPS_rename_assembly,
           OPS_delete_selected_assembly,
           OPS_copy_selected_assembly,
           OPS_connect_mesh_to_hooks_in_assembly
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
