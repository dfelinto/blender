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
import os

class FILEBROWSER_HT_fluidheader(Header):
    bl_space_type = 'FILE_BROWSER'

    def draw(self, context):
        layout = self.layout
        wm = context.window_manager
        if context.scene.mv.active_addon_name == "":
            layout.label('No Active Library Add-on',icon="ERROR")

class FILEBROWSER_MT_fd_filters(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        st = context.space_data
        params = st.params
        
        if params:
            layout.prop(params, "use_filter", text="Use Filters", icon='FILTER')
            layout.separator()
            layout.operator("fd_general.dialog_show_filters",icon='INFO')

class FILEBROWSER_MT_fd_tools(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        layout.operator("fd_library.draw_items_in_category", icon='BRUSH_DATA')

class FILEBROWSER_MT_fd_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        layout.operator("fd_library.open_active_library_path", text="Open Current Category in Windows Explorer", icon='FILE_FOLDER')
        layout.operator("fd_library.refresh_library", text="Refresh Library", icon='FILE_REFRESH')
        layout.separator()
        layout.operator("file.previous", text="Back to Previous", icon='BACK')
        layout.operator("file.parent", text="Go to Parent", icon='FILE_PARENT')
        layout.separator()
        layout.operator_context = "EXEC_DEFAULT"
        layout.operator("file.directory_new", icon='NEWFOLDER')

class FILEBROWSER_MT_fd_save(Menu):
    bl_label = "Save"

    def draw(self, context):
        layout = self.layout
        layout.operator("fd_ops.save_file_to_active_category", text="Save To Category", icon='SAVE_COPY')

class FILEBROWSER_MT_fd_navigation(Menu):
    bl_label = "Navigation"

    def draw(self, context):
        layout = self.layout
        st = context.space_data
        params = st.params
        if params:
            layout.prop(params, "use_filter", text="Use Filters", icon='FILTER')
            layout.prop(params, "use_filter_folder", text="Show Folders")
            layout.prop(params, "use_filter_blender", text="Show Blender Files")
            layout.prop(params, "use_filter_backup", text="Show Backup Files")
            layout.prop(params, "use_filter_image", text="Show Image Files")
            layout.prop(params, "use_filter_movie", text="Show Video Files")
            layout.prop(params, "use_filter_script", text="Show Script Files")
            layout.prop(params, "use_filter_font", text="Show Font Files")
            layout.prop(params, "use_filter_text", text="Show Text Files")

class FILE_MT_fd_menus(Menu):
    bl_space_type = 'VIEW3D_MT_editor_menus'
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        layout.menu("FILEBROWSER_MT_fd_view",icon='VIEWZOOM',text="     View     ")
        layout.menu("FILEBROWSER_MT_fd_filters",icon='FILTER',text="     Filters     ")
        layout.menu("FILEBROWSER_MT_fd_save",icon='FILE_TICK',text="     Save     ")
        layout.menu("FILEBROWSER_MT_fd_tools",icon='MODIFIER',text="     Tools     ")
        

class PANEL_fluid_libraries(Panel):
    bl_space_type = "FILE_BROWSER"
    bl_region_type = "TOOLS"
    bl_label = " "
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        return True

    def draw_header(self, context):
        layout = self.layout
        layout.label(" ",icon='EXTERNAL_DATA')

    def draw(self, context):
        import addon_utils
        
        layout = self.layout
        
        userpref = context.user_preferences
        used_ext = {ext.module for ext in userpref.addons}
        
        for mod in addon_utils.modules(refresh=False):
            if mod.bl_info['category'] == 'Library Add-on':
                if 'fd_drop_id' in mod.bl_info:
                    if mod.__name__ in used_ext:
                        layout.operator(mod.bl_info['fd_open_id'],
                                        text=mod.bl_info["name"],
                                        icon=mod.bl_info["icon"],
                                        emboss=True if context.scene.mv.active_addon_name == mod.bl_info["name"] else False)
        layout.separator()
        layout.menu("INFO_MT_addons",icon='BLANK1',text="   Add-ons    ")

class INFO_MT_addons(Menu):
    bl_idname = "INFO_MT_addons"
    bl_label = "Microvellum Add-ons"

    def draw(self, context):
        import addon_utils
        
        layout = self.layout
        
        userpref = context.user_preferences
        used_ext = {ext.module for ext in userpref.addons}
        
        for mod in addon_utils.modules(refresh=False):
            if mod.bl_info['category'] == 'Library Add-on':
                if mod.__name__ in used_ext:
                    layout.operator('wm.addon_disable',text=mod.bl_info["name"],icon='CHECKBOX_HLT').module = mod.__name__
                else:
                    layout.operator('wm.addon_enable',text=mod.bl_info["name"],icon='CHECKBOX_DEHLT').module = mod.__name__
        layout.separator()
        layout.operator('wm.save_userpref',text="Save User Preferences",icon='FILE_TICK')

#------REGISTER
classes = [
#            FILEBROWSER_HT_fluidheader,
           FILE_MT_fd_menus,
           FILEBROWSER_MT_fd_filters,
           FILEBROWSER_MT_fd_view,
           FILEBROWSER_MT_fd_navigation,
           FILEBROWSER_MT_fd_save,
           INFO_MT_addons,
           PANEL_fluid_libraries,
           FILEBROWSER_MT_fd_tools
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
