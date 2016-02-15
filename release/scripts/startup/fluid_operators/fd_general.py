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

import bpy, bgl, blf
from bpy.types import Header, Menu, Operator, UIList, PropertyGroup

from bpy.props import (StringProperty,
                       BoolProperty,
                       IntProperty,
                       FloatProperty,
                       FloatVectorProperty,
                       BoolVectorProperty,
                       PointerProperty,
                       EnumProperty,
                       CollectionProperty)
import os
import fd

from bpy_extras.io_utils import (ImportHelper,
                                 ExportHelper,
                                 axis_conversion,
                                 )

import itertools
from mathutils import Matrix

class OPS_drag_and_drop(Operator):
    """SPECIAL OPERATOR: This is called when you drop an image to the 3dview space"""
    bl_idname = "fd_dragdrop.drag_and_drop"
    bl_label = "Drag and Drop"
#     bl_options = {'UNDO'}
 
    #READONLY
    filepath = StringProperty(name="Filepath",
                              subtype='FILE_PATH')
    objectname = StringProperty(name="Object Name")
 
    def invoke(self, context, event):
        import addon_utils
        scene = context.scene
        for mod in addon_utils.modules(refresh=False):
            if mod.bl_info['category'] == 'Library Add-on':
                if mod.bl_info['name'] == scene.mv.active_addon_name:
                    eval('bpy.ops.' + mod.bl_info['fd_drop_id'] + '("INVOKE_DEFAULT",objectname = "' + self.objectname + '",filepath = r"' + self.filepath + '")')
        return {'FINISHED'}

class OPS_properties(Operator):
    bl_idname = "fd_general.properties"
    bl_label = "Properties"

    @classmethod
    def poll(cls, context):
        if context.object:
            return True
        else:
            return False

    def check(self,context):
        return True

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self,context,event):
        obj = context.object
        if obj:
            if obj.mv.property_id != "":
                eval('bpy.ops.' + obj.mv.property_id + '("INVOKE_DEFAULT",object_name=obj.name)')
                return {'FINISHED'}
            else:
                if context.object.type == 'CAMERA':
                    bpy.ops.fd_object.camera_properties('INVOKE_DEFAULT')
                    return{'FINISHED'}    
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)

    def draw(self, context):
        layout = self.layout
        obj = context.object
        if obj and obj.parent:
            if obj.parent.mv.type == 'BPWALL' and obj.mv.type != 'BPASSEMBLY':
                wall = fd.Wall(obj.parent)
                wall.draw_transform(layout)
                return None
        
        obj_bp = fd.get_parent_assembly_bp(obj)
        if obj_bp:
            group = fd.Assembly(obj_bp)
            group.draw_transform(layout,show_prompts=True)
        else:
            fd.draw_object_info(layout,obj)
            if obj.type == 'LAMP':
                fd.draw_object_data(layout,obj)

class OPS_load_fluid_designer_defaults(Operator):
    bl_idname = "fd_general.load_fluid_designer_defaults"
    bl_label = "Load Fluid Designer Defaults"

    @classmethod
    def poll(cls, context):
        return True
        
    def execute(self, context):
        import shutil
        path,filename = os.path.split(os.path.normpath(__file__))
        src_userpref_file = os.path.join(path,"fd_userpref.blend")
        src_startup_file = os.path.join(path,"fd_startup.blend")
        userpath = os.path.join(bpy.utils.resource_path(type='USER'),"config")
        if not os.path.exists(userpath): os.makedirs(userpath)
        dst_userpref_file = os.path.join(userpath,"fd_userpref.blend")
        dst_startup_file = os.path.join(userpath,"fd_startup.blend")
        shutil.copyfile(src_userpref_file,dst_userpref_file)
        shutil.copyfile(src_startup_file,dst_startup_file)
        return {'FINISHED'}
        
    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=550)
        
    def draw(self, context):
        layout = self.layout
        layout.label("Are you sure you want to restore the Fluid Designer default startup file and user preferences?",icon='QUESTION')
        layout.label("You will need to restart the application for the changes to take effect.",icon='BLANK1')
        
class OPS_load_blender_defaults(Operator):
    bl_idname = "fd_general.load_blender_defaults"
    bl_label = "Load Blender Defaults"
    bl_description = "This will reload the blender default startup file and user preferences"
    
    def execute(self, context):
        bpy.ops.wm.read_factory_settings()
        context.scene.mv.ui.use_default_blender_interface = True
        return {'FINISHED'}
        
    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=350)
        
    def draw(self, context):
        layout = self.layout
        layout.label("This will load a new file. You will lose any unsaved changes.",icon='QUESTION')
        layout.label("Do you want to continue?",icon='BLANK1')
        
class OPS_save_startup_file(Operator):
    bl_idname = "fd_general.save_startup_file"
    bl_label = "Save Startup File"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        bpy.ops.wm.save_homefile()
        return {'FINISHED'}
        
    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)
        
    def draw(self, context):
        layout = self.layout
        layout.label("Are you sure you want to save the current file as the startup file?",icon='QUESTION')
        layout.label("The current state of the interface will be saved as the default.",icon='BLANK1')

class OPS_open_blend_file(Operator):
    bl_idname = "fd_general.open_blend_file"
    bl_label = "Open Blend File"
    
    filepath = StringProperty(name="Filepath")
    
    @classmethod
    def poll(cls, context):
        return True
        
    def execute(self, context):
        bpy.ops.wm.open_mainfile(load_ui=False,filepath=self.filepath)
        return {'FINISHED'}
        
    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=300)
        
    def draw(self, context):
        layout = self.layout
        layout.label("Are you sure you want to open this blend file?",icon='QUESTION')
        layout.label("You will lose any unsaved changes",icon='BLANK1')
        
class OPS_change_shademode(Operator):
    bl_idname = "fd_general.change_shademode"
    bl_label = "Change Shademode"

    shade_mode = bpy.props.EnumProperty(
            name="Shade Mode",
            items=(('WIREFRAME', "Wire Frame", "WIREFRAME",'WIRE',1),
                   ('SOLID', "Solid", "SOLID",'SOLID',2),
                   ('MATERIAL', "Material","MATERIAL",'MATERIAL',3),
                   ('RENDERED', "Rendered", "RENDERED",'SMOOTH',4)
                   ),
            )

    def execute(self, context):
        context.scene.render.engine = 'CYCLES'
        context.space_data.viewport_shade = self.shade_mode
        return {'FINISHED'}
        
class OPS_dialog_show_filters(Operator):
    bl_idname = "fd_general.dialog_show_filters"
    bl_label = "Show Filters"
    bl_options = {'UNDO'}

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=300)

    def draw(self, context):
        layout = self.layout
        st = context.space_data
        params = st.params
        if params:
#             row = layout.row()
#             row.label('File Display Mode:')
#             row.prop(params, "display_type", expand=False,text="")
            layout.prop(params, "use_filter", text="Use Filters", icon='FILTER')
            layout.separator()
            box = layout.box()
            box.prop(params, "use_filter_folder", text="Show Folders")
            box.prop(params, "use_filter_blender", text="Show Blender Files")
            box.prop(params, "use_filter_backup", text="Show Backup Files")
            box.prop(params, "use_filter_image", text="Show Image Files")
            box.prop(params, "use_filter_movie", text="Show Video Files")
            box.prop(params, "use_filter_script", text="Show Script Files")
            box.prop(params, "use_filter_font", text="Show Font Files")
            box.prop(params, "use_filter_text", text="Show Text Files")
            box.prop(params, "show_hidden",text='Show Hidden Folders',icon='VISIBLE_IPO_ON')
            
class OPS_error(Operator):
    bl_idname = "fd_general.error"
    bl_label = "Fluid Designer"

    message = StringProperty(name="Message",default="Error")

    def execute(self, context):
        return {'FINISHED'} 

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=380)

    def draw(self, context):
        layout = self.layout
        layout.label(self.message)
    
class OPS_set_cursor_location(Operator):
    bl_idname = "fd_general.set_cursor_location"
    bl_label = "Cursor Location"

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self,context,event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=200)

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.prop(context.scene,"cursor_location",text="")

class OPS_start_debug(Operator):
    bl_idname = "fluidgeneral.start_debug"
    bl_label = "Start Debug"
    bl_description = "Start Debug with Eclipse"

    def execute(self, context):
        import pydevd
        pydevd.settrace()
        return {'FINISHED'}

class OPS_open_browser_window(Operator):
    bl_idname = "fd_general.open_browser_window"
    bl_label = "Open Browser Window"
    bl_description = "This will open the path that is passed in a file browser"

    path = StringProperty(name="Message",default="Error")

    def execute(self, context):
        import subprocess
        if 'Windows' in str(bpy.app.build_platform):
            subprocess.Popen(r'explorer /select,' + self.path)
        elif 'Darwin' in str(bpy.app.build_platform):
            subprocess.Popen(['open' , os.path.normpath(self.path)])
        else:
            subprocess.Popen(['open' , os.path.normpath(self.path)])
        return {'FINISHED'}

#USEFUL FOR REFERENCE
class OPS_check_for_updates(Operator):
    bl_idname = "fd_general.check_for_updates"
    bl_label = "Check For Updates"

    info = {}

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        pass
        return {'FINISHED'}
        
    def invoke(self,context,event):
        wm = context.window_manager
        import urllib.request
        url = urllib.request.urlopen("http://www.microvellum.com/AppInterfaces/FluidDesignerUpdate/CurrentUpdate.html")
        mybytes = url.read()
        mystr = mybytes.decode("utf8")
        lines = mystr.split('<p>')
        for line in lines:
            if 'CurrentVersion=' in line:
                new_line = line.replace('</p>','')
                self.info['CurrentVersion'] = new_line[len('CurrentVersion='):].strip()
            if 'InstallPath=' in line:
                new_line = line.replace('</p>','')
                self.info['InstallPath'] = new_line[len('InstallPath='):].strip()

        return wm.invoke_props_dialog(self, width=320)
        
    def draw(self, context):
        layout = self.layout
        layout.label('Current Version: ' + self.info['CurrentVersion'],icon='MOD_FLUIDSIM')
        layout.operator("wm.url_open", text="Download New Version", icon='URL').url = self.info['InstallPath']

#------REGISTER
classes = [
           OPS_drag_and_drop,
           OPS_properties,
           OPS_load_fluid_designer_defaults,
           OPS_save_startup_file,
           OPS_open_blend_file,
           OPS_dialog_show_filters,
           OPS_change_shademode,
           OPS_error,
           OPS_start_debug,
           OPS_set_cursor_location,
           OPS_load_blender_defaults,
           OPS_open_browser_window,
           OPS_check_for_updates
           ]

def register():
    for c in classes:
        bpy.utils.register_class(c)

def unregister():
    for c in classes:
        bpy.utils.unregister_class(c)

if __name__ == "__main__":
    register()
