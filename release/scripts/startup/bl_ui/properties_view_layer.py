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

# <pep8 compliant>
import bpy
from bpy.types import Panel, UIList


class ViewLayerButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "view_layer"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)


class VIEWLAYER_PT_layer(ViewLayerButtonsPanel, Panel):
    bl_label = "View Layer"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        layer = bpy.context.view_layer

        layout.prop(layer, "use", text="Use for Rendering");
        layout.prop(rd, "use_single_layer", text="Render Single Layer")


class VIEWLAYER_PT_eevee_layer_passes(ViewLayerButtonsPanel, Panel):
    bl_label = "Passes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        view_layer = context.view_layer

        split = layout.split()

        col = split.column()
        col.prop(view_layer, "use_pass_combined")
        col.prop(view_layer, "use_pass_z")
        col.prop(view_layer, "use_pass_mist")
        col.prop(view_layer, "use_pass_normal")
        col.separator()
        col.prop(view_layer, "use_pass_ambient_occlusion")

        col = split.column()
        col.label(text="Subsurface:")
        row = col.row(align=True)
        row.prop(view_layer, "use_pass_subsurface_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_subsurface_color", text="Color", toggle=True)


class VIEWLAYER_UL_override_set_collections(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.row().label(text=item.name, icon_value=icon)


class VIEWLAYER_UL_override_sets(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        row = layout.row(align=True)
        row.prop(item, "use", text="", index=index)
        row.prop(item, "name", text="", index=index, icon_value=icon, emboss=False)


class VIEWLAYER_OT_overrides(ViewLayerButtonsPanel, Panel):
    bl_label = "Overrides"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        view_layer = context.view_layer

        row = layout.row(align=True)
        row.label(text="Override Sets")

        row = layout.row()
        col = row.column()

        col.template_list("VIEWLAYER_UL_override_sets", "name", view_layer, "override_sets", view_layer.override_sets, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("scene.view_layer_override_set_add", icon='ZOOMIN', text="")
        col.operator("scene.view_layer_override_set_remove", icon='ZOOMOUT', text="")

        override_set = view_layer.override_sets.active
        if not override_set:
            return

        row = layout.row(align=True)
        row.prop(scene, "show_view_layer_overrides_scene_property", emboss=False, text="")
        row.label(text="Scene Properties")

        if scene.show_view_layer_overrides_scene_property:
            if override_set.scene_properties:
                for i, dyn_prop in enumerate(override_set.scene_properties):
                    self._draw_property(layout, dyn_prop, i, 'SCENE')
            else:
                layout.label(text="No scene property")

        row = layout.row(align=True)
        row.prop(scene, "show_view_layer_overrides_affected_collections", emboss=False, text="")
        row.label(text="Affected Collections")

        if scene.show_view_layer_overrides_affected_collections:
            row = layout.row()
            col = row.column()

            col.template_list("VIEWLAYER_UL_override_set_collections", "", override_set, "affected_collections", override_set.affected_collections, "active_index", rows=1)

            col = row.column(align=True)
            col.operator("scene.override_set_collection_link", icon='ZOOMIN', text="")
            col.operator("scene.override_set_collection_unlink", icon='ZOOMOUT', text="")

        row = layout.row(align=True)
        row.prop(scene, "show_view_layer_overrides_collections_property", emboss=False, text="")
        row.label(text="Collection Properties")

        if scene.show_view_layer_overrides_collections_property:
            if override_set.collection_properties:
                for i, dyn_prop in enumerate(override_set.collection_properties):
                    self._draw_property(layout, dyn_prop, i, 'COLLECTION')
            else:
                layout.label(text="No collection property")

    def _draw_property(self, layout, dyn_prop, index, property_type):
        box = layout.box()
        row = box.row()
        row.prop(dyn_prop, "use", text="")
        subrow = row.row()
        subrow.active = dyn_prop.use
        subrow.label(text=dyn_prop.name, icon='NONE')
        subrow.prop(dyn_prop, "override_mode", text="")
        subrow.prop(dyn_prop, "value_int", text="")
        ops = row.operator("scene.view_layer_override_remove", text="", icon='ZOOMOUT', emboss=False)
        ops.index = index
        ops.property_type = property_type


classes = (
    VIEWLAYER_PT_layer,
    VIEWLAYER_PT_eevee_layer_passes,
    VIEWLAYER_UL_override_set_collections,
    VIEWLAYER_UL_override_sets,
    VIEWLAYER_OT_overrides,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
