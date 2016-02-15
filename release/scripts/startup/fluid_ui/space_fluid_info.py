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
from bpy.types import Header, Menu
import os

class INFO_HT_fluidheader(Header):
    bl_space_type = 'INFO'

    def draw(self, context):
        layout = self.layout

        window = context.window
        scene = context.scene
        if not context.scene.mv.ui.use_default_blender_interface:
            if scene.mv.ui.show_debug_tools:
                layout.prop(scene.mv.ui,"show_debug_tools",icon='DISCLOSURE_TRI_DOWN',text="",emboss=False)
                layout.prop(scene.mv.ui,"use_default_blender_interface",icon='BLENDER',text="")
                layout.operator("wm.console_toggle", icon='CONSOLE',text="")
                layout.operator("fluidgeneral.start_debug", icon='GAME',text="")
            else:
                layout.prop(scene.mv.ui,"show_debug_tools",icon='DISCLOSURE_TRI_RIGHT',text="",emboss=False)
        
            row = layout.row(align=True)
            INFO_MT_fd_menus.draw_collapsible(context, layout)
            if window.screen.show_fullscreen:
                layout.operator("screen.back_to_previous", icon='SCREEN_BACK', text="Back to Previous")
                layout.separator()
            layout.separator()
            if context.active_object:
                if context.active_object.mode == 'EDIT':
                    layout.label('You are currently in Edit Mode type the TAB key to return to object mode',icon='ERROR')
    
            layout.template_running_jobs()
    
            layout.template_reports_banner()
    
            row = layout.row(align=True)
    
            if bpy.app.autoexec_fail is True and bpy.app.autoexec_fail_quiet is False:
                layout.operator_context = 'EXEC_DEFAULT'
                row.label("Auto-run disabled: %s" % bpy.app.autoexec_fail_message, icon='ERROR')
                if bpy.data.is_saved:
                    props = row.operator("wm.open_mainfile", icon='SCREEN_BACK', text="Reload Trusted")
                    props.filepath = bpy.data.filepath
                    props.use_scripts = True
    
                row.operator("script.autoexec_warn_clear", text="Ignore")
                return
            
            row.label(text=scene.statistics(), translate=False)
            
class INFO_MT_fd_menus(Menu):
    bl_idname = "INFO_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        layout.menu("INFO_MT_fluidfile",icon='FILE_FOLDER',text="   File   ")
        layout.menu("INFO_MT_edit",icon='RECOVER_AUTO',text="   Edit   ")
        layout.menu("INFO_MT_create_rendering",icon='RENDER_STILL',text="   Render    ")
        layout.menu("INFO_MT_fluidhelp",icon='HELP',text="   Help   ")

class INFO_MT_fluidfile(Menu):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.read_homefile", text="New", icon='NEW')
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER').load_ui = False
        layout.menu("INFO_MT_file_open_recent", icon='OPEN_RECENT')
        layout.operator("wm.recover_last_session", icon='RECOVER_LAST')
        layout.operator("wm.recover_auto_save", text="Recover Auto Save...", icon='RECOVER_AUTO')

        layout.separator()

        layout.operator_context = 'EXEC_AREA' if context.blend_data.is_saved else 'INVOKE_AREA'
        layout.operator("wm.save_mainfile", text="Save", icon='FILE_TICK')

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save As...", icon='SAVE_AS')
        
        #unity 
#         layout.operator("fd_scene.create_unity_build", text="Create Unity Build", icon='RADIO')
        
        layout.separator()
        layout.operator("fd_material.clear_all_materials_from_file", text="Clear Materials", icon='MATERIAL')
        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.link", text="Link", icon='LINK_BLEND')
        layout.operator("wm.append", text="Append", icon='APPEND_BLEND')

        layout.separator()

        layout.operator("screen.userpref_show", text="User Preferences...", icon='PREFERENCES')
        layout.menu("INFO_MT_units",icon='LINENUMBERS_ON',text="Units")
        layout.separator()
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("fd_general.save_startup_file", icon='SAVE_PREFS')
        
        layout.separator()
        
        layout.operator_context = 'INVOKE_AREA'
        
        layout.operator("fd_general.load_fluid_designer_defaults", icon='LOAD_FACTORY')
        layout.operator("fd_general.load_blender_defaults", icon='BLENDER')

        layout.separator()

        layout.menu("INFO_MT_file_import", icon='IMPORT')
        layout.menu("INFO_MT_file_export", icon='EXPORT')
        
        layout.separator()

        layout.menu("INFO_MT_file_external_data", icon='EXTERNAL_DATA')

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.quit_blender", text="Quit", icon='QUIT')

class INFO_MT_file_import(Menu):
    bl_idname = "INFO_MT_file_import"
    bl_label = "Import"

    def draw(self, context):
        if bpy.app.build_options.collada:
            self.layout.operator("wm.collada_import", text="Collada (Default) (.dae)")

class INFO_MT_file_export(Menu):
    bl_idname = "INFO_MT_file_export"
    bl_label = "Export"

    def draw(self, context):
        if bpy.app.build_options.collada:
            self.layout.operator("wm.collada_export", text="Collada (Default) (.dae)")

class INFO_MT_edit(Menu):
    bl_idname = "INFO_MT_edit"
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout
        layout.operator("ed.undo",icon='LOOP_BACK')
        layout.operator("ed.redo",icon='LOOP_FORWARDS')
        layout.operator("ed.undo_history",icon='RECOVER_LAST')

class INFO_MT_file_external_data(Menu):
    bl_label = "External Data"

    def draw(self, context):
        layout = self.layout

        layout.operator("file.pack_all", text="Pack into .blend file")
        layout.operator("file.unpack_all", text="Unpack into Files")

        layout.separator()

        layout.operator("file.make_paths_relative")
        layout.operator("file.make_paths_absolute")
        layout.operator("file.report_missing_files")
        layout.operator("file.find_missing_files")

class INFO_MT_interface(Menu):
    bl_label = "Interface"

    def draw(self, context):
        layout = self.layout
        layout.prop(context.scene.mv.ui,"use_default_blender_interface",icon='BLENDER',text="Use Default Blender Interface")
        layout.prop(context.scene.mv.ui,"show_debug_tools",icon='BLENDER',text="Use Debug Tools")

class INFO_MT_window(Menu):
    bl_label = "Window"

    def draw(self, context):
        import sys

        layout = self.layout

        layout.operator("wm.window_duplicate")
        layout.operator("wm.window_fullscreen_toggle", icon='FULLSCREEN_ENTER')

        layout.separator()

        layout.operator("screen.screenshot").full = True
        layout.operator("screen.screencast").full = True

        if sys.platform[:3] == "win":
            layout.separator()
            layout.operator("wm.console_toggle", icon='CONSOLE')

class INFO_MT_units(Menu):
    bl_idname = "INFO_MT_units"
    bl_label = "Scenes"

    def draw(self, context):
        unit = context.scene.unit_settings
        layout = self.layout
        layout.prop_menu_enum(unit, "system")
        layout.prop_menu_enum(unit, "system_rotation")
        
class INFO_MT_fluidhelp(Menu):
    bl_label = "Fluid Help"

    def draw(self, context):
        layout = self.layout

        layout.operator("wm.url_open", text="Microvellum e-Support", icon='HELP').url = "http://support.microvellum.com"
        layout.operator("wm.url_open", text="Microvellum Website", icon='URL').url = "http://www.microvellum.com"
        layout.separator()
        layout.operator("wm.url_open", text="Blender Manual", icon='HELP').url = "http://wiki.blender.org/index.php/Doc:2.6/Manual"
        layout.operator("wm.url_open", text="Blender Release Log", icon='URL').url = "http://www.blender.org/development/release-logs/blender-269"
        layout.separator()
        layout.operator("wm.url_open", text="Blender Website", icon='URL').url = "http://www.blender.org"
        layout.operator("wm.url_open", text="Python API Reference", icon='URL').url = bpy.types.WM_OT_doc_view._prefix
        layout.operator("wm.operator_cheat_sheet", icon='TEXT')
        layout.operator("wm.sysinfo", icon='TEXT')
        layout.separator()
        layout.operator("wm.splash", icon='BLENDER')

class INFO_MT_create_rendering(Menu):
    bl_label = "Rendering"

    def draw(self, context):
        layout = self.layout
        layout.operator("fd_scene.render_scene",text="Render Scene",icon='RENDER_STILL')
        layout.operator("render.opengl", text="Render OpenGL")
        layout.operator("fd_scene.add_thumbnail_camera_and_lighting",text="Add Thumbnail Camera and Lighting",icon='RENDER_RESULT')
        layout.separator()
        layout.operator("fd_scene.render_settings",text="Render Settings",icon='INFO')

class INFO_MT_thumbnail(Menu):
    bl_label = "Thumbnails"

    def draw(self, context):
        layout = self.layout
        layout.operator("fd_object.add_thumbnail_camera",text="Add Thumbnail Camera and Lighting",icon='RENDER_STILL')
        layout.operator("fd_scene.render_scene",text="Render Product Thumbnail",icon='RENDER_RESULT')

classes = [
           INFO_MT_fluidfile,
           INFO_MT_fd_menus,
		   INFO_HT_fluidheader,
           INFO_MT_units,
           INFO_MT_fluidhelp,
           INFO_MT_interface,
           INFO_MT_create_rendering,
           INFO_MT_edit,
           INFO_MT_thumbnail
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
