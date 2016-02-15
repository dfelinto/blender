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
from bpy.types import Header, Menu, Panel

import fd

class VIEW3D_HT_fluidheader(Header):
    bl_space_type = 'VIEW_3D'

    def draw(self, context):
        layout = self.layout

        if not context.scene.mv.ui.use_default_blender_interface:
            if context.scene.layers[0] != True:
                layout.label('Some layers are not visible',icon='ERROR')
                layout.operator('view3d.layers',text="Show All Layers").nr = 0

            obj = context.active_object
            toolsettings = context.tool_settings
    
            row = layout.row(align=True)
            row.template_header()
            
            VIEW3D_MT_fd_menus.draw_collapsible(context, layout)
            
            if context.space_data.viewport_shade == 'WIREFRAME':
                layout.operator_menu_enum("fd_general.change_shademode","shade_mode",text="Wire Frame",icon='WIRE')
            if context.space_data.viewport_shade == 'SOLID':
                layout.operator_menu_enum("fd_general.change_shademode","shade_mode",text="Solid        ",icon='SOLID')
            if context.space_data.viewport_shade == 'MATERIAL':
                layout.operator_menu_enum("fd_general.change_shademode","shade_mode",text="Material     ",icon='MATERIAL')
            if context.space_data.viewport_shade == 'RENDERED':
                layout.operator_menu_enum("fd_general.change_shademode","shade_mode",text="Rendered    ",icon='SMOOTH')
            
            row = layout.row(align=True)
            row.prop(context.space_data,"show_manipulator",text="")
            row.prop(context.space_data,"transform_manipulators",text="")
            row.prop(context.space_data,"transform_orientation",text="")
        
            if not obj or obj.mode not in {'SCULPT', 'VERTEX_PAINT', 'WEIGHT_PAINT', 'TEXTURE_PAINT'}:
                snap_element = toolsettings.snap_element
                row = layout.row(align=True)
                row.prop(toolsettings, "use_snap", text="")
                row.prop(toolsettings, "snap_element", text="", icon_only=True)
                if snap_element != 'INCREMENT':
                    row.prop(toolsettings, "snap_target", text="")
                    if obj:
                        if obj.mode in {'OBJECT', 'POSE'} and snap_element != 'VOLUME':
                            row.prop(toolsettings, "use_snap_align_rotation", text="")
                        elif obj.mode == 'EDIT':
                            row.prop(toolsettings, "use_snap_self", text="")
    
                if snap_element == 'VOLUME':
                    row.prop(toolsettings, "use_snap_peel_object", text="")
                elif snap_element == 'FACE':
                    row.prop(toolsettings, "use_snap_project", text="")

            row = layout.row(align=True)
            row.operator("view3d.ruler",text="",icon='CURVE_NCURVE')

class VIEW3D_MT_fd_menus(Menu):
    bl_space_type = 'VIEW3D_MT_editor_menus'
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        layout.menu("VIEW3D_MT_fluidview",icon='VIEWZOOM',text="   View   ")
        layout.menu("INFO_MT_fluidaddobject",icon='GREASEPENCIL',text="   Add   ")
        layout.menu("VIEW3D_MT_fluidtools",icon='MODIFIER',text="   Tools   ")
        layout.menu("VIEW3D_MT_selectiontools",icon='RESTRICT_SELECT_OFF',text="   Select   ")
        layout.menu("MENU_cursor_tools",icon='CURSOR',text="   Cursor   ")

class PANEL_object_properties(Panel):
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_label = " "
    bl_options = {'DEFAULT_CLOSED'}
    
    @classmethod
    def poll(cls, context):
        if context.object:
            return True
        else:
            return False

    def draw_header(self, context):
        layout = self.layout
        obj = context.object
        layout.label(text="Object: " + obj.name,icon='OBJECT_DATA')

    def draw(self, context):
        layout = self.layout
        obj = context.object
        if obj:
            fd.draw_object_properties(layout,obj)
                
class PANEL_assembly_properties(Panel):
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_label = " "
    bl_options = {'DEFAULT_CLOSED'}
    
    @classmethod
    def poll(cls, context):
        obj_bp = fd.get_assembly_bp(context.object)
        if obj_bp:
            return True
        else:
            return False

    def draw_header(self, context):
        layout = self.layout
        obj_bp = fd.get_assembly_bp(context.object)
        if obj_bp:
            layout.label(text='Assembly: ' + obj_bp.mv.name_object,icon='OUTLINER_DATA_LATTICE')

    def draw(self, context):
        layout = self.layout
        obj_bp = fd.get_assembly_bp(context.object)
        if obj_bp:
            group = fd.Assembly(obj_bp)
            group.draw_properties(layout)

class PANEL_wall_properties(Panel):
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_label = " "
    bl_options = {'DEFAULT_CLOSED'}
    
    @classmethod
    def poll(cls, context):
        obj_bp = fd.get_wall_bp(context.object)
        if obj_bp:
            return True
        else:
            return False

    def draw_header(self, context):
        layout = self.layout
        obj_bp = fd.get_wall_bp(context.object)
        if obj_bp:
            layout.label(text="Wall: " + obj_bp.name,icon='SNAP_PEEL_OBJECT')

    def draw(self, context):
        layout = self.layout
        obj_bp = fd.get_wall_bp(context.object)
        if obj_bp:
            group = fd.Assembly(obj_bp)
            box = layout.box()
            group.draw_transform(box)

class VIEW3D_MT_fluidview(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.view_all",icon='VIEWZOOM')
        layout.operator("view3d.view_selected",text="Zoom To Selected",icon='ZOOM_SELECTED')

        layout.separator()

        layout.operator("view3d.viewnumpad", text="Camera",icon='CAMERA_DATA').type = 'CAMERA'
        layout.operator("view3d.viewnumpad", text="Top",icon='TRIA_DOWN').type = 'TOP'
        layout.operator("view3d.viewnumpad", text="Front",icon='TRIA_UP').type = 'FRONT'
        layout.operator("view3d.viewnumpad", text="Left",icon='TRIA_LEFT').type = 'LEFT'
        layout.operator("view3d.viewnumpad", text="Right",icon='TRIA_RIGHT').type = 'RIGHT'

        layout.separator()

        layout.operator("view3d.view_persportho",icon='SCENE')

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.separator()

        layout.operator("screen.area_dupli",icon='GHOST')
        layout.operator("screen.region_quadview",icon='VIEW3D_VEC')
        layout.operator("screen.screen_full_area",icon='FULLSCREEN_ENTER')
        
        layout.separator()
        
        layout.menu("MENU_viewport_settings",icon='SCRIPTPLUGINS',text="Viewport Settings")
        

class INFO_MT_fluidaddobject(Menu):
    bl_label = "Add Object"

    def draw(self, context):
        layout = self.layout

        # note, don't use 'EXEC_SCREEN' or operators wont get the 'v3d' context.

        # Note: was EXEC_AREA, but this context does not have the 'rv3d', which prevents
        #       "align_view" to work on first call (see [#32719]).
        layout.operator_context = 'INVOKE_REGION_WIN'
        
        layout.operator("fd_assembly.add_wall",icon='SNAP_PEEL_OBJECT')
        layout.operator("fd_object.create_floor_plane", icon='MESH_GRID')
        layout.separator()
        layout.operator("fd_assembly.add_assembly", icon='OUTLINER_DATA_LATTICE')

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.separator()
        layout.menu("INFO_MT_mesh_add", icon='OUTLINER_OB_MESH')

        #layout.operator_menu_enum("object.curve_add", "type", text="Curve", icon='OUTLINER_OB_CURVE')
        layout.menu("INFO_MT_curve_add", icon='OUTLINER_OB_CURVE')
        #layout.operator_menu_enum("object.surface_add", "type", text="Surface", icon='OUTLINER_OB_SURFACE')
        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("object.text_add", text="Text", icon='OUTLINER_OB_FONT')
        layout.separator()

        layout.operator_menu_enum("object.empty_add", "type", text="Empty", icon='OUTLINER_OB_EMPTY')
        layout.separator()
        layout.operator("fd_object.add_camera",text="Camera",icon='OUTLINER_OB_CAMERA')
        layout.menu("MENU_add_lamp", icon='OUTLINER_OB_LAMP')
        layout.separator()
        layout.menu("MENU_add_grease_pencil", icon='GREASEPENCIL')
        layout.separator()
        
        if len(bpy.data.groups) > 10:
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator("object.group_instance_add", text="Group Instance...", icon='OUTLINER_OB_EMPTY')
        else:
            layout.operator_menu_enum("object.group_instance_add", "group", text="Group Instance", icon='OUTLINER_OB_EMPTY')

class VIEW3D_MT_fluidtools(Menu):
    bl_context = "objectmode"
    bl_label = "Object"

    def draw(self, context):
        layout = self.layout
        layout.menu("VIEW3D_MT_objecttools",icon='OBJECT_DATA')
        layout.menu("VIEW3D_MT_grouptools",icon='GROUP')
        layout.menu("VIEW3D_MT_assemblytools",icon='OUTLINER_DATA_LATTICE')
        
#         layout.menu("VIEW3D_MT_dimensiontools",icon='ALIGN') FIX THESE TOOLS

class VIEW3D_MT_transformtools(Menu):
    bl_context = "objectmode"
    bl_label = "Transforms"

    def draw(self, context):
        layout = self.layout
        layout.operator("transform.translate",text='Grab',icon='MAN_TRANS')
        layout.operator("transform.rotate",icon='MAN_ROT')
        layout.operator("transform.resize",text="Scale",icon='MAN_SCALE')

class VIEW3D_MT_selectiontools(Menu):
    bl_context = "objectmode"
    bl_label = "Selection"

    def draw(self, context):
        layout = self.layout
        if context.active_object:
            if context.active_object.mode == 'OBJECT':
                layout.operator("object.select_all",text='Toggle De/Select',icon='MAN_TRANS')
            else:
                layout.operator("mesh.select_all",text='Toggle De/Select',icon='MAN_TRANS')
        else:
            layout.operator("object.select_all",text='Toggle De/Select',icon='MAN_TRANS')
        layout.operator("view3d.select_border",icon='BORDER_RECT')
        layout.operator("view3d.select_circle",icon='BORDER_LASSO')

class VIEW3D_MT_origintools(Menu):
    bl_context = "objectmode"
    bl_label = "Origin Tools"

    def draw(self, context):
        layout = self.layout
        layout.operator("object.origin_set",text="Origin to Cursor",icon='CURSOR').type = 'ORIGIN_CURSOR'
        layout.operator("object.origin_set",text="Origin to Geometry",icon='CLIPUV_HLT').type = 'ORIGIN_GEOMETRY'

class VIEW3D_MT_shadetools(Menu):
    bl_context = "objectmode"
    bl_label = "Object Shading"

    def draw(self, context):
        layout = self.layout
        layout.operator("object.shade_smooth",icon='SOLID')
        layout.operator("object.shade_flat",icon='SNAP_FACE')

class VIEW3D_MT_objecttools(Menu):
    bl_context = "objectmode"
    bl_label = "Object Tools"

    def draw(self, context):
        layout = self.layout
        layout.menu("VIEW3D_MT_transformtools",icon='MAN_TRANS')
        layout.separator()
        layout.operator("object.duplicate_move",icon='PASTEDOWN')
        layout.operator("object.convert", text="Convert to Mesh",icon='MOD_REMESH').target = 'MESH'
        layout.operator("object.join",icon='ROTATECENTER')
        layout.separator()
        layout.menu("VIEW3D_MT_origintools",icon='SPACE2')
        layout.separator()
        layout.menu("VIEW3D_MT_shadetools",icon='MOD_MULTIRES')
        layout.separator()
        layout.operator("object.delete",icon='X').use_global = False

class VIEW3D_MT_grouptools(Menu):
    bl_context = "objectmode"
    bl_label = "Group Tools"

    def draw(self, context):
        layout = self.layout
        layout.operator("fd_group.make_group_from_selection",icon='GROUP')
        
class VIEW3D_MT_assemblytools(Menu):
    bl_context = "objectmode"
    bl_label = "Assembly Tools"

    def draw(self, context):
        layout = self.layout
        layout.operator("fd_assembly.make_group_from_selected_assembly",icon='GROUP')
        layout.separator()
        layout.operator("fd_assembly.copy_selected_assembly",icon='PASTEDOWN')
        layout.operator("fd_assembly.select_selected_assemby_base_point",icon='MAN_TRANS')
        layout.operator('fd_assembly.delete_selected_assembly',icon='X')
        
class VIEW3D_MT_extrusiontools(Menu):
    bl_context = "objectmode"
    bl_label = "Extrusion Tools"

    def draw(self, context):
        layout = self.layout
        layout.operator("fd_ops.extrude_curve", icon='IPO_CONSTANT')

class VIEW3D_MT_dimensiontools(Menu):
    bl_context = "objectmode"
    bl_label = "Dimension Tools"

    def draw(self, context):
        layout = self.layout
        layout.operator("fd_group.add_visual_dimensions", icon='ALIGN')
        layout.operator("fd_group.edit_dimension_global_properties", icon='VPAINT_HLT')

class MENU_cursor_tools(Menu):
    bl_label = "Cursor Tools"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("fd_general.set_cursor_location",icon='CURSOR',text="Set Cursor Location")
        layout.separator()
        layout.operator("view3d.snap_cursor_to_selected",icon='CURSOR')
        layout.operator("view3d.snap_cursor_to_center",icon='VIEW3D_VEC')
        layout.operator("view3d.snap_selected_to_cursor",icon='SPACE2')
        layout.separator()
        layout.prop(context.space_data,"pivot_point",text="")

class MENU_mesh_selection(Menu):
    bl_label = "Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator("mesh.select_mode",text="Vertex Select",icon='VERTEXSEL').type='VERT'
        layout.operator("mesh.select_mode",text="Edge Select",icon='EDGESEL').type='EDGE'
        layout.operator("mesh.select_mode",text="Face Select",icon='FACESEL').type='FACE'

class MENU_delete_selection(Menu):
    bl_label = "Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator("mesh.delete",text="Delete Vertices",icon='VERTEXSEL').type='VERT'
        layout.operator("mesh.delete",text="Delete Edges",icon='EDGESEL').type='EDGE'
        layout.operator("mesh.delete",text="Delete Faces",icon='FACESEL').type='FACE'
        layout.operator("mesh.delete",text="Delete Only Faces",icon='FACESEL').type='ONLY_FACE'
        
class MENU_delete_selection_curve(Menu):
    bl_label = "Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator("curve.delete",text="Delete Vertices",icon='VERTEXSEL').type='VERT'
        layout.operator("curve.delete",text="Delete Edges",icon='EDGESEL').type='SEGMENT'
        
class MENU_right_click_menu_edit_mesh(Menu):
    bl_label = "Menu"

    def draw(self, context):
        obj = context.active_object
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.menu("MENU_mesh_selection",text="Selection Mode",icon='MAN_TRANS')
        layout.separator()
        layout.operator("view3d.edit_mesh_extrude_move_normal",icon='CURVE_PATH')
        layout.operator("mesh.knife_tool",icon='SCULPTMODE_HLT')
        layout.operator("mesh.subdivide",icon='OUTLINER_OB_LATTICE')
        layout.operator("mesh.loopcut_slide",icon='SNAP_EDGE')
        layout.operator("transform.edge_slide",icon='SNAP_EDGE')
        layout.operator("mesh.bevel",icon='MOD_BEVEL')
        layout.operator("mesh.edge_face_add",icon='SNAP_FACE')
        layout.operator("mesh.separate",icon='UV_ISLANDSEL').type = 'SELECTED'
        layout.operator("mesh.remove_doubles",icon='MOD_DISPLACE')
        layout.separator()
        layout.operator("uv.smart_project",icon='ASSET_MANAGER',text="Unwrap Mesh")
        layout.separator()
        layout.prop(obj.data,'show_extra_edge_length')
        layout.prop(obj.data,'show_extra_face_angle')
        layout.prop(obj.data,'show_extra_face_area')
        layout.separator()
        layout.menu("MENU_delete_selection",text="Delete",icon='X')
        layout.separator()
        layout.operator("fd_object.toggle_edit_mode",text="Exit Edit Mode",icon=fd.const.icon_editmode).object_name = obj.name
        
class MENU_handel_type(Menu):
    bl_label = "Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator("curve.handle_type_set",icon='CURVE_PATH')
        
class MENU_right_click_menu_edit_curve(Menu):
    bl_label = "Menu"

    def draw(self, context):
        obj = context.active_object
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("curve.extrude_move",icon='CURVE_PATH')
        layout.operator("curve.switch_direction",icon='SCULPTMODE_HLT')
        layout.operator("curve.subdivide",icon='OUTLINER_OB_LATTICE')
        layout.prop(obj.data,'show_handles')
        layout.separator()
        layout.operator("curve.handle_type_set",icon='CURVE_PATH')
        layout.separator()
        layout.menu("MENU_delete_selection_curve",text="Delete",icon='X')
        layout.separator()
        layout.operator("fd_object.toggle_edit_mode",text="Exit Edit Mode",icon=fd.const.icon_editmode).object_name = obj.name
        
class MENU_add_assembly_object(Menu):
    bl_label = "Add Assembly Object"
    
    def draw(self, context):
        layout = self.layout
        
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("fd_assembly.add_mesh_to_assembly",icon=fd.const.icon_mesh,text="Add Mesh")
        layout.operator("fd_assembly.add_empty_to_assembly",icon=fd.const.icon_empty,text="Add Empty")
        layout.operator("fd_assembly.add_curve_to_assembly",icon=fd.const.icon_curve,text="Add Curve")
        layout.operator("fd_assembly.add_text_to_assembly",icon=fd.const.icon_font,text="Add Text")

class MENU_add_grease_pencil(Menu):
    bl_label = "Grease Pencil"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("gpencil.draw", text="Draw",icon='GREASEPENCIL').mode = 'DRAW'
        layout.operator("gpencil.draw", text="Line",icon='ZOOMOUT').mode = 'DRAW_STRAIGHT'
        layout.operator("gpencil.draw", text="Poly",icon='IPO_CONSTANT').mode = 'DRAW_POLY'
        layout.operator("gpencil.draw", text="Erase",icon='X').mode = 'ERASER'
        layout.separator()
        layout.prop(context.tool_settings, "use_grease_pencil_sessions")
        
class MENU_viewport_settings(Menu):
    bl_label = "Viewport Settings"

    def draw(self, context):
        layout = self.layout
        if context.space_data.type == 'VIEW_3D':
            layout.prop(context.space_data,"show_floor",text="Show Grid Floor")
            layout.prop(context.space_data,"show_axis_x",text="Show X Axis")
            layout.prop(context.space_data,"show_axis_y",text="Show Y Axis")
            layout.prop(context.space_data,"show_axis_z",text="Show Z Axis")
            layout.prop(context.space_data,"show_only_render",text="Only Render")
            layout.prop(context.space_data,"show_relationship_lines",text="Show Relationship Lines")
            layout.prop(context.space_data,"grid_lines",text="Grid Lines")
            layout.prop(context.space_data,"grid_scale",text="Grid Scale")
            layout.prop(context.space_data,"lens",text="Viewport lens angle")
            layout.prop(context.space_data,"lock_camera",text="Lock Camera to View")

class MENU_add_lamp(Menu):
    bl_label = "Lamp"

    def draw(self, context):
        layout = self.layout
        layout.operator("object.lamp_add",icon='LAMP_POINT',text="Add Point Lamp").type = 'POINT'
        layout.operator("object.lamp_add",icon='LAMP_SUN',text="Add Sun Lamp").type = 'SUN'
        layout.operator("object.lamp_add",icon='LAMP_SPOT',text="Add Spot Lamp").type = 'SPOT'
        layout.operator("object.lamp_add",icon='LAMP_AREA',text="Add Area Lamp").type = 'AREA'

class MENU_active_group_options(Menu):
    bl_label = "Lamp"

    def draw(self, context):
        layout = self.layout
        obj_bp = fd.get_parent_assembly_bp(context.object)
        if obj_bp:
            for index, page in enumerate(obj_bp.mv.PromptPage.COL_MainTab):
                if page.type == 'VISIBLE':
                    props = layout.operator("fd_prompts.show_object_prompts",icon='SETTINGS',text=page.name)
                    props.object_bp_name = obj_bp.name
                    props.tab_name = page.name
                    props.index = index
                if page.type == 'CALCULATOR':
                    props = layout.operator("fd_prompts.show_object_prompts",icon='SETTINGS',text=page.name)
                    props.object_bp_name = obj_bp.name
                    props.tab_name = page.name
                    props.index = index

class MENU_active_group_prompts(Menu):
    bl_label = "Lamp"

    def draw(self, context):
        layout = self.layout
        obj_bp = fd.get_parent_assembly_bp(context.object)
        if obj_bp:
            if len(obj_bp.mv.PromptPage.COL_MainTab) == 0:
                layout.label('This group contains no prompts')
            else:
                for index, page in enumerate(obj_bp.mv.PromptPage.COL_MainTab):
                    props = layout.operator("fd_prompts.show_object_prompts",icon='SETTINGS',text=page.name)
                    props.object_bp_name = obj_bp.name
                    props.tab_name = page.name
                    props.index = index

#------REGISTER
classes = [
           VIEW3D_HT_fluidheader,
           VIEW3D_MT_fd_menus,
           PANEL_object_properties,
           PANEL_assembly_properties,
           PANEL_wall_properties,
           VIEW3D_MT_fluidview,
           VIEW3D_MT_fluidtools,
           VIEW3D_MT_grouptools,
           VIEW3D_MT_assemblytools,
           MENU_viewport_settings,
           INFO_MT_fluidaddobject,
           MENU_cursor_tools,
           MENU_mesh_selection,
           MENU_delete_selection,
           MENU_delete_selection_curve,
           MENU_right_click_menu_edit_mesh,
           MENU_right_click_menu_edit_curve,
           MENU_add_assembly_object,
           VIEW3D_MT_origintools,
           VIEW3D_MT_shadetools,
           VIEW3D_MT_objecttools,
           VIEW3D_MT_extrusiontools,
           VIEW3D_MT_dimensiontools,
           MENU_add_grease_pencil,
           VIEW3D_MT_transformtools,
           VIEW3D_MT_selectiontools,
           MENU_add_lamp,
           MENU_active_group_options,
           MENU_active_group_prompts
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
