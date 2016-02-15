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

from bpy.app.handlers import persistent
import os
from . import driver_functions
import inspect

@persistent
def set_default_user_prefs(scene):
    bpy.context.user_preferences.system.use_scripts_auto_execute = True
    bpy.context.window_manager.mv.wall_rotation = 0

@persistent
def load_driver_functions(scene):
    for name, obj in inspect.getmembers(driver_functions):
        if name not in bpy.app.driver_namespace:
            bpy.app.driver_namespace[name] = obj

# @persistent
# def set_default_library(scene):
#     for lib in bpy.context.window_manager.mv.libraries:
#         if lib.name == 'Cabinet Library':
#             eval("bpy.ops." + lib.open_library_id + "()")
#             bpy.context.window_manager.mv.active_library_name = lib.name

bpy.app.handlers.load_post.append(set_default_user_prefs)
bpy.app.handlers.load_post.append(load_driver_functions)
# bpy.app.handlers.load_post.append(set_default_library)

def register():
    import sys
    
    if os.path.exists(r'C:\Program Files\eclipse\plugins\org.python.pydev_2.8.2.2013090511\pysrc'):
        PYDEV_SOURCE_DIR = r'C:\Program Files\eclipse\plugins\org.python.pydev_2.8.2.2013090511\pysrc'
        if sys.path.count(PYDEV_SOURCE_DIR) < 1:
            sys.path.append(PYDEV_SOURCE_DIR)    
            
    elif os.path.exists(r'C:\Program Files (x86)\eclipse\plugins\org.python.pydev_2.8.2.2013090511\pysrc'):
        PYDEV_SOURCE_DIR = r'C:\Program Files (x86)\eclipse\plugins\org.python.pydev_2.8.2.2013090511\pysrc'
        if sys.path.count(PYDEV_SOURCE_DIR) < 1:
            sys.path.append(PYDEV_SOURCE_DIR)    
    else:
        print("NO DEBUG ATTACHED")
    
def unregister():
    pass
