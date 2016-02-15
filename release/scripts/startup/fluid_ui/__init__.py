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

def register():
    from . import lists
    from . import space_fluid_file
    from . import space_fluid_info
    from . import space_fluid_view3d

    lists.register()
    space_fluid_file.register()
    space_fluid_info.register()
    space_fluid_view3d.register()

def unregister():

    from . import lists
    from . import space_fluid_file
    from . import space_fluid_info
    from . import space_fluid_view3d

    lists.unregister()
    space_fluid_file.unregister()
    space_fluid_info.unregister()
    space_fluid_view3d.unregister()