/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_layer.c
 *  \ingroup RNA
 */

#include "DNA_scene_types.h"
#include "DNA_layer_types.h"
#include "DNA_view3d_types.h"

#include "BLI_math.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "ED_object.h"
#include "ED_render.h"

#include "RE_engine.h"

#include "DRW_engine.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#include "DNA_group_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"

#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_mesh.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

/***********************************/


static PointerRNA rna_LayerCollections_active_collection_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	LayerCollection *lc = view_layer->active_collection;
	return rna_pointer_inherit_refine(ptr, &RNA_LayerCollection, lc);
}

static void rna_LayerCollections_active_collection_set(PointerRNA *ptr, PointerRNA value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	LayerCollection *lc = (LayerCollection *)value.data;
	const int index = BKE_layer_collection_findindex(view_layer, lc);
	if (index != -1) BKE_layer_collection_activate(view_layer, lc);
}

static PointerRNA rna_LayerObjects_active_object_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Object, view_layer->basact ? view_layer->basact->object : NULL);
}

static void rna_LayerObjects_active_object_set(PointerRNA *ptr, PointerRNA value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	if (value.data)
		view_layer->basact = BKE_view_layer_base_find(view_layer, (Object *)value.data);
	else
		view_layer->basact = NULL;
}

static char *rna_ViewLayer_path(PointerRNA *ptr)
{
	ViewLayer *srl = (ViewLayer *)ptr->data;
	char name_esc[sizeof(srl->name) * 2];

	BLI_strescape(name_esc, srl->name, sizeof(name_esc));
	return BLI_sprintfN("view_layers[\"%s\"]", name_esc);
}

static IDProperty *rna_ViewLayer_idprops(PointerRNA *ptr, bool create)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;

	if (create && !view_layer->id_properties) {
		IDPropertyTemplate val = {0};
		view_layer->id_properties = IDP_New(IDP_GROUP, &val, "ViewLayer ID properties");
	}

	return view_layer->id_properties;
}

static void rna_ViewLayer_update_render_passes(ID *id)
{
	Scene *scene = (Scene *)id;
	if (scene->nodetree)
		ntreeCompositUpdateRLayers(scene->nodetree);
}

static PointerRNA rna_ViewLayer_objects_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;

	/* we are actually iterating a ObjectBase list */
	Base *base = (Base *)internal->link;
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, base->object);
}

static int rna_ViewLayer_objects_selected_skip(CollectionPropertyIterator *iter, void *UNUSED(data))
{
	ListBaseIterator *internal = &iter->internal.listbase;
	Base *base = (Base *)internal->link;

	if ((base->flag & BASE_SELECTED) != 0) {
		return 0;
	}

	return 1;
};

static PointerRNA rna_ViewLayer_depsgraph_get(PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	if (GS(id->name) == ID_SCE) {
		Scene *scene = (Scene *)id;
		ViewLayer *view_layer = (ViewLayer *)ptr->data;
		Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, false);
		return rna_pointer_inherit_refine(ptr, &RNA_Depsgraph, depsgraph);
	}
	return PointerRNA_NULL;
}

static void rna_LayerObjects_selected_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	rna_iterator_listbase_begin(iter, &view_layer->object_bases, rna_ViewLayer_objects_selected_skip);
}

static void rna_ViewLayer_update_tagged(ID *id_ptr, ViewLayer *view_layer, Main *bmain)
{
#ifdef WITH_PYTHON
	/* Allow drivers to be evaluated */
	BPy_BEGIN_ALLOW_THREADS;
#endif

	Scene *scene = (Scene *)id_ptr;
	Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
	BKE_scene_graph_update_tagged(depsgraph, bmain);

#ifdef WITH_PYTHON
	BPy_END_ALLOW_THREADS;
#endif
}

static void rna_ObjectBase_select_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Base *base = (Base *)ptr->data;
	short mode = (base->flag & BASE_SELECTED) ? BA_SELECT : BA_DESELECT;
	ED_object_base_select(base, mode);
}

static void rna_OverriddenCollection_name_get(PointerRNA *ptr, char *value)
{
	Collection *collection = ((AffectedCollection *)ptr->data)->collection;
	strcpy(value, collection->id.name + 2);
}

static int rna_OverriddenCollection_name_length(PointerRNA *ptr)
{
	Collection *collection = ((AffectedCollection *)ptr->data)->collection;
	return strnlen(collection->id.name + 2, sizeof(collection->id.name));
}

static int rna_OverriddenCollections_active_index_get(PointerRNA *ptr)
{
	OverrideSet *override_set = (OverrideSet *)ptr->data;
	return override_set->active_affected_collection;
}

static void rna_OverriddenCollections_active_index_set(PointerRNA *ptr, int value)
{
	OverrideSet *override_set = (OverrideSet *)ptr->data;
	override_set->active_affected_collection = value;
}

static void rna_OverriddenCollections_active_index_index_range(
        PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
	OverrideSet *override_set = (OverrideSet *)ptr->data;
	*min = 0;
	*max = max_ii(0, BLI_listbase_count(&override_set->affected_collections) - 1);
}

static PointerRNA rna_OverriddenCollections_active_get(PointerRNA *ptr)
{
	OverrideSet *override_set = (OverrideSet *)ptr->data;
	LinkData *link = BLI_findlink(&override_set->affected_collections, override_set->active_affected_collection);
	return rna_pointer_inherit_refine(ptr, &RNA_OverriddenCollection, link);
}

static void rna_OverriddenCollections_active_set(PointerRNA *ptr, PointerRNA value)
{
	OverrideSet *override_set = (OverrideSet *)ptr->data;
	LinkData *link = (LinkData *)value.data;

	const int index = BLI_findindex(&override_set->affected_collections, link);
	if (index != -1) {
		override_set->active_affected_collection = index;
	}
}

static void rna_OverriddenCollection_link(
        ID *id, OverrideSet *override_set, Main *bmain, bContext *C, ReportList *reports, Collection *collection)
{
	if (!BKE_view_layer_override_set_collection_link(override_set, collection)) {
		BKE_reportf(reports,
		            RPT_ERROR,
		            "Collection '%s' already affected by override set '%s'",
		            collection->id.name + 2,
		            override_set->name);
		return;
	}

	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = (Scene *)id;
	DEG_graph_id_tag_update(bmain, depsgraph, &scene->id, DEG_TAG_COPY_ON_WRITE);
	DEG_graph_id_type_tag_update(bmain, depsgraph, ID_OB, DEG_TAG_COPY_ON_WRITE);
	WM_main_add_notifier(NC_SCENE | ND_DYN_OVERRIDES, scene);
}

static void rna_OverriddenCollection_unlink(
        ID *id, OverrideSet *override_set, Main *bmain, bContext *C, ReportList *reports, Collection *collection)
{
	if (!BKE_view_layer_override_set_collection_unlink(override_set, collection)) {
		BKE_reportf(reports,
		            RPT_ERROR,
		            "Collection '%s' is not affected by override set '%s'",
		            collection->id.name + 2,
		            override_set->name);
		return;
	}

	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = (Scene *)id;
	DEG_graph_id_tag_update(bmain, depsgraph, &scene->id, DEG_TAG_COPY_ON_WRITE);
	DEG_graph_id_type_tag_update(bmain, depsgraph, ID_OB, DEG_TAG_COPY_ON_WRITE);
	WM_main_add_notifier(NC_SCENE | ND_DYN_OVERRIDES, scene);
}

static char *rna_OverrideSet_path(PointerRNA *ptr)
{
	OverrideSet *override_set = (OverrideSet *)ptr->data;
	return BLI_sprintfN("view_layer.override_sets[\"%s\"]", override_set->name);
}

static void rna_OverrideSet_name_set(PointerRNA *ptr, const char *value)
{
	Scene *scene = (Scene *)ptr->id.data;
	OverrideSet *override_set = (OverrideSet *)ptr->data;

	ViewLayer *view_layer = BKE_view_layer_find_from_override_set(scene, override_set);
	BLI_strncpy_utf8(override_set->name, value, sizeof(override_set->name));
	BLI_uniquename(&view_layer->override_sets,
	               override_set,
	               DATA_("OverrideSet"),
	               '.',
	               offsetof(OverrideSet, name),
	               sizeof(override_set->name));
}

static void  rna_OverrideSet_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	OverrideSet *override_set = (OverrideSet *)ptr->data;
	ViewLayer *view_layer = BKE_view_layer_find_from_override_set(scene, override_set);
	Depsgraph *depsgraph =
	        (Depsgraph *)BKE_scene_get_depsgraph(scene,
	                                             view_layer,
	                                             false);

	DEG_graph_id_tag_update(bmain, depsgraph, &scene->id, DEG_TAG_COPY_ON_WRITE);
	DEG_graph_id_type_tag_update(bmain, depsgraph, ID_OB, DEG_TAG_COPY_ON_WRITE);
}

static int rna_OverrideSets_active_override_set_index_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	return view_layer->active_override_set;
}

static void rna_OverrideSets_active_override_set_index_set(PointerRNA *ptr, int value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	view_layer->active_override_set = value;
}

static void rna_OverrideSets_active_override_set_index_range(
        PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;

	*min = 0;
	*max = max_ii(0, BLI_listbase_count(&view_layer->override_sets) - 1);
}

static PointerRNA rna_OverrideSets_active_override_set_get(PointerRNA *ptr)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	OverrideSet *override_set = BLI_findlink(&view_layer->override_sets, view_layer->active_override_set);
	return rna_pointer_inherit_refine(ptr, &RNA_OverrideSet, override_set);
}

static void rna_OverrideSets_active_override_set_set(PointerRNA *ptr, PointerRNA value)
{
	ViewLayer *view_layer = (ViewLayer *)ptr->data;
	OverrideSet *override_set = (OverrideSet *)value.data;
	const int index = BLI_findindex(&view_layer->override_sets, override_set);
	if (index != -1) {
		view_layer->active_override_set = index;
	}
}

static OverrideSet *rna_OverrideSet_new(ID *id, ViewLayer *view_layer, const char *name)
{
	Scene *scene = (Scene *)id;
	OverrideSet *override_set = BKE_view_layer_override_set_add(view_layer, name);
	DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE);
	WM_main_add_notifier(NC_SCENE | ND_DYN_OVERRIDES, scene);
	return override_set;
}

static void rna_OverrideSet_remove(
        ID *id, ViewLayer *view_layer, Main *UNUSED(bmain), ReportList *reports, PointerRNA *ptr)
{
	OverrideSet *override_set = ptr->data;
	Scene *scene = (Scene *)id;

	if (!BKE_view_layer_override_set_remove(view_layer, override_set)) {
		BKE_reportf(reports,
		            RPT_ERROR,
		            "Override set'%s' could not be removed from view layer '%s'",
		            override_set->name,
		            view_layer->name);
		return;
	}

	RNA_POINTER_INVALIDATE(ptr);
	DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE);
	WM_main_add_notifier(NC_SCENE | ND_DYN_OVERRIDES, scene);
}

static void rna_LayerCollection_use_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->id.data;
	LayerCollection *lc = (LayerCollection *)ptr->data;
	ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, lc);

	BKE_layer_collection_sync(scene, view_layer);

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);
	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, NULL);
}

#else

static void rna_def_layer_collection(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LayerCollection", NULL);
	RNA_def_struct_ui_text(srna, "Layer Collection", "Layer collection");
	RNA_def_struct_ui_icon(srna, ICON_GROUP);

	prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
	RNA_def_property_struct_type(prop, "Collection");
	RNA_def_property_ui_text(prop, "Collection", "Collection this layer collection is wrapping");

	prop = RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layer_collections", NULL);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_ui_text(prop, "Children", "Child layer collections");

	prop = RNA_def_property(srna, "exclude", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LAYER_COLLECTION_EXCLUDE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Exclude", "Exclude collection from view layer");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, "rna_LayerCollection_use_update");
}

static void rna_def_layer_collections(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "LayerCollections");
	srna = RNA_def_struct(brna, "LayerCollections", NULL);
	RNA_def_struct_sdna(srna, "ViewLayer");
	RNA_def_struct_ui_text(srna, "Layer Collections", "Collections of render layer");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_pointer_funcs(prop, "rna_LayerCollections_active_collection_get",
	                               "rna_LayerCollections_active_collection_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Active Layer Collection", "Active Layer Collection");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);
}

static void rna_def_layer_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "LayerObjects");
	srna = RNA_def_struct(brna, "LayerObjects", NULL);
	RNA_def_struct_sdna(srna, "ViewLayer");
	RNA_def_struct_ui_text(srna, "Layer Objects", "Collections of objects");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, "rna_LayerObjects_active_object_get", "rna_LayerObjects_active_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Object", "Active object for this layer");
	/* Could call: ED_object_base_activate(C, rl->basact);
	 * but would be a bad level call and it seems the notifier is enough */
	RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, NULL);

	prop = RNA_def_property(srna, "selected", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, "rna_LayerObjects_selected_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_ViewLayer_objects_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Selected Objects", "All the selected objects of this layer");
}

static void rna_def_object_base(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ObjectBase", NULL);
	RNA_def_struct_sdna(srna, "Base");
	RNA_def_struct_ui_text(srna, "Object Base", "An object instance in a render layer");
	RNA_def_struct_ui_icon(srna, ICON_OBJECT_DATA);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_ui_text(prop, "Object", "Object this base links to");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BASE_SELECTED);
	RNA_def_property_ui_text(prop, "Select", "Object base selection state");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ObjectBase_select_update");
}

static void rna_def_overridden_collection(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "OverriddenCollection", NULL);
	RNA_def_struct_ui_text(srna, "Overridden Collection", "Collection overridden by override set");
	RNA_def_struct_ui_icon(srna, ICON_COLLAPSEMENU);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop,
	                              "rna_OverriddenCollection_name_get",
	                              "rna_OverriddenCollection_name_length",
	                              NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "Collection name");
}

static void rna_def_overridden_collections(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "OverriddenCollections");
	srna = RNA_def_struct(brna, "OverriddenCollections", NULL);
	RNA_def_struct_sdna(srna, "OverrideSet");
	RNA_def_struct_ui_text(srna, "Overridden Collections", "Collections overridden by override set");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop,
	                           "rna_OverriddenCollections_active_index_get",
	                           "rna_OverriddenCollections_active_index_set",
	                           "rna_OverriddenCollections_active_index_index_range");
	RNA_def_property_ui_text(prop, "Active Affected Collection Index", "Active index in override set array");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "OverriddenCollection");
	RNA_def_property_pointer_funcs(prop,
	                               "rna_OverriddenCollections_active_get",
	                               "rna_OverriddenCollections_active_set",
	                               NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Active Affected Collection", "Active affected collection in override set array");

	func = RNA_def_function(srna, "link", "rna_OverriddenCollection_link");
	RNA_def_function_ui_description(func, "Link collection to override set");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "collection", "Collection", "", "Collection to link to override set");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	func = RNA_def_function(srna, "unlink", "rna_OverriddenCollection_unlink");
	RNA_def_function_ui_description(func, "Unlink a collection from override set");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "collection", "Collection", "", "Collection to unlink from override set");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

	rna_def_overridden_collection(brna);
}

static void rna_def_override_set(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "OverrideSet", NULL);
	RNA_def_struct_ui_text(srna, "Override Set", "Set of overrides");
	RNA_def_struct_path_func(srna, "rna_OverrideSet_path");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_OverrideSet_name_set");
	RNA_def_property_ui_text(prop, "Name", "Override set name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", DYN_OVERRIDE_SET_USE);
	RNA_def_property_ui_text(prop, "Enabled", "Disable or enable the override set");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_OverrideSet_update");

	prop = RNA_def_property(srna, "affected_collections", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "affected_collections", NULL);
	RNA_def_property_struct_type(prop, "OverriddenCollection");
	RNA_def_property_ui_text(prop, "Affected Collections", "Collections that overridden by override set");
	rna_def_overridden_collections(brna, prop);

	prop = RNA_def_property(srna, "scene_properties", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "scene_properties", NULL);
	RNA_def_property_struct_type(prop, "DynamicOverrideProperty");
	RNA_def_property_ui_text(prop, "Dynamic Override Properties", "Properties overriden for the entire scene");

	prop = RNA_def_property(srna, "collection_properties", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "collection_properties", NULL);
	RNA_def_property_struct_type(prop, "DynamicOverrideProperty");
	RNA_def_property_ui_text(prop,
	                         "Dynamic Override Properties",
	                         "Properties overriden for the objects in the affected collections");
}

static void rna_def_override_sets(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "OverrideSets");
	srna = RNA_def_struct(brna, "OverrideSets", NULL);
	RNA_def_struct_sdna(srna, "ViewLayer");
	RNA_def_struct_ui_text(srna, "Override Sets", "Sets of view layer overrides");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop,
	                           "rna_OverrideSets_active_override_set_index_get",
	                           "rna_OverrideSets_active_override_set_index_set",
	                           "rna_OverrideSets_active_override_set_index_range");
	RNA_def_property_ui_text(prop, "Active Override Set Index", "Active index in override set array");
	RNA_def_property_update(prop, NC_SCENE | ND_DYN_OVERRIDES, NULL);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "OverrideSet");
	RNA_def_property_pointer_funcs(prop,
	                               "rna_OverrideSets_active_override_set_get",
	                               "rna_OverrideSets_active_override_set_set",
	                               NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Active Override Set", "Active Override Set");
	RNA_def_property_update(prop, NC_SCENE | ND_DYN_OVERRIDES, NULL);

	func = RNA_def_function(srna, "new", "rna_OverrideSet_new");
	RNA_def_function_ui_description(func, "Add an override set to view layer");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the override set");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "result", "OverrideSet", "", "Newly created override set");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_OverrideSet_remove");
	RNA_def_function_ui_description(func, "Remove an override set from view layer");
	RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
	parm = RNA_def_pointer(func, "override_set", "OverrideSet", "", "Override set to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	rna_def_override_set(brna);
}

void RNA_def_view_layer(BlenderRNA *brna)
{
	FunctionRNA *func;
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ViewLayer", NULL);
	RNA_def_struct_ui_text(srna, "View Layer", "View layer");
	RNA_def_struct_ui_icon(srna, ICON_RENDER_RESULT);
	RNA_def_struct_path_func(srna, "rna_ViewLayer_path");
	RNA_def_struct_idprops_func(srna, "rna_ViewLayer_idprops");

	rna_def_view_layer_common(srna, 1);

	func = RNA_def_function(srna, "update_render_passes", "rna_ViewLayer_update_render_passes");
	RNA_def_function_ui_description(func, "Requery the enabled render passes from the render engine");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF);

	prop = RNA_def_property(srna, "collections", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layer_collections", NULL);
	RNA_def_property_struct_type(prop, "LayerCollection");
	RNA_def_property_ui_text(prop, "Layer Collections", "");
	rna_def_layer_collections(brna, prop);

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_ViewLayer_objects_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Objects", "All the objects in this layer");
	rna_def_layer_objects(brna, prop);

	prop = RNA_def_property(srna, "override_sets", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "override_sets", NULL);
	RNA_def_property_struct_type(prop, "OverrideSet");
	RNA_def_property_ui_text(prop, "Override Sets", "");
	rna_def_override_sets(brna, prop);

	/* layer options */
	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VIEW_LAYER_RENDER);
	RNA_def_property_ui_text(prop, "Enabled", "Disable or enable the render layer");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	prop = RNA_def_property(srna, "use_freestyle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VIEW_LAYER_FREESTYLE);
	RNA_def_property_ui_text(prop, "Freestyle", "Render stylized strokes in this Layer");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

	/* Freestyle */
	rna_def_freestyle_settings(brna);

	prop = RNA_def_property(srna, "freestyle_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "freestyle_config");
	RNA_def_property_struct_type(prop, "FreestyleSettings");
	RNA_def_property_ui_text(prop, "Freestyle Settings", "");

	/* debug update routine */
	func = RNA_def_function(srna, "update", "rna_ViewLayer_update_tagged");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
	RNA_def_function_ui_description(func,
	                                "Update data tagged to be updated from previous access to data or operators");

	/* Dependency Graph */
	prop = RNA_def_property(srna, "depsgraph", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Depsgraph");
	RNA_def_property_ui_text(prop, "Dependency Graph", "Dependencies in the scene data");
	RNA_def_property_pointer_funcs(prop, "rna_ViewLayer_depsgraph_get", NULL, NULL, NULL);

	/* Nested Data  */
	/* *** Non-Animated *** */
	RNA_define_animate_sdna(false);
	rna_def_layer_collection(brna);
	rna_def_object_base(brna);
	RNA_define_animate_sdna(true);
	/* *** Animated *** */
}

#endif
