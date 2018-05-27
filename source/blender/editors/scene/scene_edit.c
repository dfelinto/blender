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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/scene/scene_edit.c
 *  \ingroup edscene
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_compiler_attrs.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"
#include "DNA_workspace_types.h"

#include "ED_object.h"
#include "ED_render.h"
#include "ED_scene.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

/* prototypes */
typedef struct OverrideSetLinkCollectionData OverrideSetLinkCollectionData;
static void view_layer_override_set_collection_link_menus_items(struct uiLayout *layout, struct OverrideSetLinkCollectionData *menu);

Scene *ED_scene_add(Main *bmain, bContext *C, wmWindow *win, eSceneCopyMethod method)
{
	Scene *scene_new;

	if (method == SCE_COPY_NEW) {
		scene_new = BKE_scene_add(bmain, DATA_("Scene"));
	}
	else { /* different kinds of copying */
		Scene *scene_old = WM_window_get_active_scene(win);

		scene_new = BKE_scene_copy(bmain, scene_old, method);

		/* these can't be handled in blenkernel currently, so do them here */
		if (method == SCE_COPY_LINK_DATA) {
			ED_object_single_users(bmain, scene_new, false, true);
		}
		else if (method == SCE_COPY_FULL) {
			ED_editors_flush_edits(C, false);
			ED_object_single_users(bmain, scene_new, true, true);
		}
	}

	WM_window_change_active_scene(bmain, C, win, scene_new);

	WM_event_add_notifier(C, NC_SCENE | ND_SCENEBROWSE, scene_new);

	return scene_new;
}

/**
 * \note Only call outside of area/region loops
 * \return true if successful
 */
bool ED_scene_delete(bContext *C, Main *bmain, wmWindow *win, Scene *scene)
{
	Scene *scene_new;

	if (scene->id.prev)
		scene_new = scene->id.prev;
	else if (scene->id.next)
		scene_new = scene->id.next;
	else
		return false;

	WM_window_change_active_scene(bmain, C, win, scene_new);

	BKE_libblock_remap(bmain, scene, scene_new, ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_NEVER_NULL_USAGE);

	id_us_clear_real(&scene->id);
	if (scene->id.us == 0) {
		BKE_libblock_free(bmain, scene);
	}

	return true;
}

static ViewLayer *scene_change_get_new_view_layer(const WorkSpace *workspace, const Scene *scene_new)
{
	ViewLayer *layer_new = BKE_workspace_view_layer_exists(workspace, scene_new);
	return layer_new ? layer_new : BKE_view_layer_default_view(scene_new);
}

void ED_scene_change_update(
        Main *bmain, bContext *C,
        wmWindow *win, const bScreen *screen, Scene *UNUSED(scene_old), Scene *scene_new)
{
	WorkSpace *workspace = CTX_wm_workspace(C);
	ViewLayer *layer_new = scene_change_get_new_view_layer(workspace, scene_new);
	Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene_new, layer_new, true);
	Object *obact_new = OBACT(layer_new);
	UNUSED_VARS(obact_new);

#if 0
	/* mode syncing */
	eObjectMode object_mode_old = workspace->object_mode;
	ViewLayer *layer_old = BKE_view_layer_from_workspace_get(scene_old, workspace);
	Object *obact_old = OBACT(layer_old);
	UNUSED_VARS(obact_old, object_mode_old);
#endif

	win->scene = scene_new;
	CTX_data_scene_set(C, scene_new);
	BKE_workspace_view_layer_set(workspace, layer_new, scene_new);
	BKE_scene_set_background(bmain, scene_new);
	DEG_graph_relations_update(depsgraph, bmain, scene_new, layer_new);
	DEG_on_visible_update(bmain, false);

	ED_screen_update_after_scene_change(screen, scene_new, layer_new);
	ED_render_engine_changed(bmain);
	ED_update_for_newframe(bmain, depsgraph);

	/* complete redraw */
	WM_event_add_notifier(C, NC_WINDOW, NULL);
}

static bool view_layer_remove_poll(
        const Scene *scene, const ViewLayer *layer)
{
	const int act = BLI_findindex(&scene->view_layers, layer);

	if (act == -1) {
		return false;
	}
	else if ((scene->view_layers.first == scene->view_layers.last) &&
	         (scene->view_layers.first == layer))
	{
		/* ensure 1 layer is kept */
		return false;
	}

	return true;
}

static void view_layer_remove_unset_nodetrees(const Main *bmain, Scene *scene, ViewLayer *layer)
{
	int act_layer_index = BLI_findindex(&scene->view_layers, layer);

	for (Scene *sce = bmain->scene.first; sce; sce = sce->id.next) {
		if (sce->nodetree) {
			BKE_nodetree_remove_layer_n(sce->nodetree, scene, act_layer_index);
		}
	}
}

bool ED_scene_view_layer_delete(
        Main *bmain, Scene *scene, ViewLayer *layer,
        ReportList *reports)
{
	if (view_layer_remove_poll(scene, layer) == false) {
		if (reports) {
			BKE_reportf(reports, RPT_ERROR, "View layer '%s' could not be removed from scene '%s'",
			            layer->name, scene->id.name + 2);
		}

		return false;
	}

	/* We need to unset nodetrees before removing the layer, otherwise its index will be -1. */
	view_layer_remove_unset_nodetrees(bmain, scene, layer);

	BLI_remlink(&scene->view_layers, layer);
	BLI_assert(BLI_listbase_is_empty(&scene->view_layers) == false);

	ED_workspace_view_layer_unset(bmain, scene, layer, scene->view_layers.first);

	BKE_view_layer_free(layer);

	BKE_workspace_view_layer_remove(bmain, layer);

	DEG_id_tag_update(&scene->id, 0);
	DEG_relations_tag_update(bmain);
	WM_main_add_notifier(NC_SCENE | ND_LAYER | NA_REMOVED, scene);

	return true;
}

static int scene_new_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win = CTX_wm_window(C);
	int type = RNA_enum_get(op->ptr, "type");

	ED_scene_add(bmain, C, win, type);

	return OPERATOR_FINISHED;
}

static void SCENE_OT_new(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{SCE_COPY_NEW, "NEW", 0, "New", "Add new scene"},
		{SCE_COPY_EMPTY, "EMPTY", 0, "Copy Settings", "Make a copy without any objects"},
		{SCE_COPY_LINK_OB, "LINK_OBJECTS", 0, "Link Objects", "Link to the objects from the current scene"},
		{SCE_COPY_LINK_DATA, "LINK_OBJECT_DATA", 0, "Link Object Data", "Copy objects linked to data from the current scene"},
		{SCE_COPY_FULL, "FULL_COPY", 0, "Full Copy", "Make a full copy of the current scene"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "New Scene";
	ot->description = "Add new scene by type";
	ot->idname = "SCENE_OT_new";

	/* api callbacks */
	ot->exec = scene_new_exec;
	ot->invoke = WM_menu_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

static int scene_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);

	if (ED_scene_delete(C, CTX_data_main(C), CTX_wm_window(C), scene) == false) {
		return OPERATOR_CANCELLED;
	}

	if (G.debug & G_DEBUG)
		printf("scene delete %p\n", scene);

	WM_event_add_notifier(C, NC_SCENE | NA_REMOVED, scene);

	return OPERATOR_FINISHED;
}

static void SCENE_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Scene";
	ot->description = "Delete active scene";
	ot->idname = "SCENE_OT_delete";

	/* api callbacks */
	ot->exec = scene_delete_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int view_layer_override_set_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	BKE_view_layer_override_set_add(CTX_data_view_layer(C), "New Override Set");
	DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE);
	WM_event_add_notifier(C, NC_SCENE | ND_DYN_OVERRIDES, scene);
	return OPERATOR_FINISHED;
}

static void SCENE_OT_view_layer_override_set_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add View Layer Override Set";
	ot->description = "Add a view layer override set";
	ot->idname = "SCENE_OT_view_layer_override_set_add";

	/* api callbacks */
	ot->exec = view_layer_override_set_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int view_layer_override_set_remove_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	OverrideSet *override_set = BLI_findlink(&view_layer->override_sets, view_layer->active_override_set);

	if (override_set == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No override set found");
		return OPERATOR_CANCELLED;
	}

	BKE_view_layer_override_set_remove(view_layer, override_set);

	DEG_graph_id_tag_update(bmain, depsgraph, &scene->id, DEG_TAG_COPY_ON_WRITE);
	DEG_graph_id_type_tag_update(bmain, depsgraph, ID_OB, DEG_TAG_COPY_ON_WRITE);

	WM_event_add_notifier(C, NC_SCENE | ND_DYN_OVERRIDES, scene);
	return OPERATOR_FINISHED;
}

static int view_layer_override_set_remove_poll(bContext *C)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	return view_layer->active_override_set || BLI_listbase_count_at_most(&view_layer->override_sets, 1);
}

static void SCENE_OT_view_layer_override_set_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove View Layer Override Set";
	ot->description = "Remove active view layer override set";
	ot->idname = "SCENE_OT_view_layer_override_set_remove";

	/* api callbacks */
	ot->exec = view_layer_override_set_remove_exec;
	ot->poll = view_layer_override_set_remove_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

#define COLLECTION_INVALID_INDEX -1

static int view_layer_override_set_collection_link_poll(bContext *C)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	return view_layer->active_override_set || BLI_findlink(&view_layer->override_sets, view_layer->active_override_set);
}

static int view_layer_override_set_collection_link_exec(bContext *C, wmOperator *op)
{
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "collection_index");

	if (!RNA_property_is_set(op->ptr, prop)) {
		BKE_report(op->reports, RPT_ERROR, "No collection selected");
		return OPERATOR_CANCELLED;
	}

	int collection_index = RNA_property_int_get(op->ptr, prop);
	Scene *scene = CTX_data_scene(C);
	Collection *collection = BKE_collection_from_index(scene, collection_index);

	if (collection == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Unexpected error, collection not found");
		return OPERATOR_CANCELLED;
	}

	ViewLayer *view_layer = CTX_data_view_layer(C);
	OverrideSet *override_set = BLI_findlink(&view_layer->override_sets, view_layer->active_override_set);

	if (BKE_view_layer_override_set_collection_link(override_set, collection)) {
		DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE);
		WM_event_add_notifier(C, NC_SCENE | ND_DYN_OVERRIDES, scene);
		return OPERATOR_FINISHED;
	}
	else {
		BKE_reportf(op->reports,
		            RPT_ERROR,
		            "Collection '%s' already affected by override set '%s'",
		            collection->id.name + 2,
		            override_set->name);
		return OPERATOR_CANCELLED;
	}
}

typedef struct OverrideSetLinkCollectionData {
	struct OverrideSetLinkCollectionData *next, *prev;
	int index;
	struct Collection *collection;
	struct ListBase submenus;
	PointerRNA ptr;
	struct wmOperatorType *ot;
} OverrideSetLinkCollectionData;

static int view_layer_override_set_collection_link_menus_create(wmOperator *op, OverrideSetLinkCollectionData *menu)
{
	int index = menu->index;
	for (CollectionChild *child= menu->collection->children.first;
	     child != NULL;
	     child = child->next)
	{
		OverrideSetLinkCollectionData *submenu = MEM_callocN (sizeof(OverrideSetLinkCollectionData),
		                                            "OverrideSetLinkCollectionData submenu - expected memleak");
		BLI_addtail(&menu->submenus, submenu);
		submenu->collection = child->collection;
		submenu->index = ++index;
		index = view_layer_override_set_collection_link_menus_create(op, submenu);
		submenu->ot = op->type;
	}
	return index;
}

static void view_layer_override_set_collection_link_menus_free_recursive(OverrideSetLinkCollectionData *menu)
{
	for (OverrideSetLinkCollectionData *submenu = menu->submenus.first;
	     submenu != NULL;
	     submenu = submenu->next)
	{
		view_layer_override_set_collection_link_menus_free_recursive(submenu);
	}
	BLI_freelistN(&menu->submenus);
}

static void view_layer_override_set_collection_link_menus_free(OverrideSetLinkCollectionData **menu)
{
	if (*menu == NULL) {
		return;
	}

	view_layer_override_set_collection_link_menus_free_recursive(*menu);
	MEM_freeN(*menu);
	*menu = NULL;
}

static void view_layer_override_set_collection_link_menu_create(bContext *UNUSED(C), uiLayout *layout, void *menu_v)
{
	OverrideSetLinkCollectionData *menu = menu_v;

	uiItemIntO(layout,
	           menu->collection->id.name + 2,
	           ICON_NONE,
	           "SCENE_OT_override_set_collection_link",
	           "collection_index",
	           menu->index);
	uiItemS(layout);

	for (OverrideSetLinkCollectionData *submenu = menu->submenus.first;
	     submenu != NULL;
	     submenu = submenu->next)
	{
		view_layer_override_set_collection_link_menus_items(layout, submenu);
	}
}

static void view_layer_override_set_collection_link_menus_items(uiLayout *layout, OverrideSetLinkCollectionData *menu)
{
	if (BLI_listbase_is_empty(&menu->submenus)) {
		uiItemIntO(layout,
		           menu->collection->id.name + 2,
		           ICON_NONE,
		           "SCENE_OT_override_set_collection_link",
		           "collection_index",
		           menu->index);
	}
	else {
		uiItemMenuF(layout,
		            menu->collection->id.name + 2,
		            ICON_NONE,
		            view_layer_override_set_collection_link_menu_create,
		            menu);
	}
}

/* This is allocated statically because we need this available for the menus creation callback. */
static OverrideSetLinkCollectionData *master_collection_menu = NULL;

static int view_layer_override_set_collection_link_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	/* Reset the menus data for the current master collection, and free previously allocated data. */
	view_layer_override_set_collection_link_menus_free(&master_collection_menu);

	PropertyRNA *prop;
	prop = RNA_struct_find_property(op->ptr, "collection_index");
	if (RNA_property_is_set(op->ptr, prop)) {
		return view_layer_override_set_collection_link_exec(C, op);
	}

	Collection *master_collection = BKE_collection_master(CTX_data_scene(C));

	/* We need the data to be allocated so it's available during menu drawing.
	 * Technically we could use wmOperator->customdata. However there is no free callback
	 * called to an operator that exit with OPERATOR_INTERFACE to launch a menu.
	 *
	 * So we are left with a memory that will necessarily leak. It's a small leak though.*/
	if (master_collection_menu == NULL) {
		master_collection_menu = MEM_callocN(sizeof(OverrideSetLinkCollectionData),
		                                     "OverrideSetLinkCollectionData menu - expected eventual memleak");
	}

	master_collection_menu->collection = master_collection;
	master_collection_menu->ot = op->type;
	view_layer_override_set_collection_link_menus_create(op, master_collection_menu);

	uiPopupMenu *pup;
	uiLayout *layout;

	/* Build the menus. */
	pup = UI_popup_menu_begin(C, IFACE_("Link Collection"), ICON_NONE);
	layout = UI_popup_menu_layout(pup);

	/* We use invoke here so we can read ctrl from event. */
	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

	view_layer_override_set_collection_link_menu_create(C, layout, master_collection_menu);

	UI_popup_menu_end(C, pup);

	return OPERATOR_INTERFACE;
}

static void SCENE_OT_override_set_collection_link(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Link Collection to Override Set";
	ot->description = "Link a collection to a view layer override set";
	ot->idname = "SCENE_OT_override_set_collection_link";

	/* api callbacks */
	ot->exec = view_layer_override_set_collection_link_exec;
	ot->invoke = view_layer_override_set_collection_link_invoke;
	ot->poll = view_layer_override_set_collection_link_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_int(ot->srna, "collection_index", COLLECTION_INVALID_INDEX, COLLECTION_INVALID_INDEX, INT_MAX,
	                   "Collection Index", "Index of the collection to link", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

#undef COLLECTION_INVALID_INDEX

static int view_layer_override_set_collection_unlink_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	OverrideSet *override_set = BLI_findlink(&view_layer->override_sets, view_layer->active_override_set);
	AffectedCollection *affected_collection = BLI_findlink(&override_set->affected_collections, override_set->active_affected_collection);
	Collection *collection = affected_collection->collection;

	BKE_view_layer_override_set_collection_unlink(override_set, collection);

	DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE);
	WM_event_add_notifier(C, NC_SCENE | ND_DYN_OVERRIDES, scene);
	return OPERATOR_FINISHED;
}

static int view_layer_override_set_collection_unlink_poll(bContext *C)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	OverrideSet *override_set = BLI_findlink(&view_layer->override_sets, view_layer->active_override_set);

	if (override_set == NULL) {
		return 0;
	}

	return override_set->active_affected_collection || BLI_listbase_count_at_most(&override_set->affected_collections, 1);
}

static void SCENE_OT_override_set_collection_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unlink Collection from Override Set";
	ot->description = "Unlink collection from view layer override set";
	ot->idname = "SCENE_OT_override_set_collection_unlink";

	/* api callbacks */
	ot->exec = view_layer_override_set_collection_unlink_exec;
	ot->poll = view_layer_override_set_collection_unlink_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static struct {
	PointerRNA ptr;
	PropertyRNA *prop;
	int index;
	bool set;
} override_property_data = { .set = false };

static int view_layer_override_add_exec(bContext *C, wmOperator *op)
{
	if (override_property_data.set == false) {
		UI_context_active_but_prop_get(C,
		                               &override_property_data.ptr,
		                               &override_property_data.prop,
		                               &override_property_data.index);
	}
	override_property_data.set = false;

	ID *id = override_property_data.ptr.id.data;
	BLI_assert(id != NULL);

	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	OverrideSet *override_set;

	if (RNA_boolean_get(op->ptr, "is_new")) {
		char new_override_set_name[MAX_NAME];
		RNA_string_get(op->ptr, "new_override_set_name", new_override_set_name);
		override_set = BKE_view_layer_override_set_add(view_layer, new_override_set_name);
	}
	else if (BLI_listbase_is_empty(&view_layer->override_sets)) {
		override_set = BKE_view_layer_override_set_add(view_layer, "Override Set");
	}
	else {
		const int override_set_index = RNA_int_get(op->ptr, "override_set_index");
		override_set = BLI_findlink(&view_layer->override_sets, override_set_index);
	}

	if (override_set == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No valid override set selected");
		return OPERATOR_CANCELLED;
	}

	BKE_view_layer_override_property_add(override_set,
	                                     &override_property_data.ptr,
	                                     override_property_data.prop,
	                                     override_property_data.index);

	DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE);
	WM_event_add_notifier(C, NC_SCENE | ND_DYN_OVERRIDES, scene);
	return OPERATOR_FINISHED;
}

static int view_layer_override_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	PropertyRNA *prop;
	ViewLayer *view_layer = CTX_data_view_layer(C);

	if (RNA_boolean_get(op->ptr, "is_new")) {
		prop = RNA_struct_find_property(op->ptr, "new_override_set_name");
		if (!RNA_property_is_set(op->ptr, prop)) {
			/* The dialog popup messes with the context prop/ptr, so we need to pre-store it
			 * to re-access it from the exec function. */
			UI_context_active_but_prop_get(C,
			                               &override_property_data.ptr,
			                               &override_property_data.prop,
			                               &override_property_data.index);
			override_property_data.set = true;
			return WM_operator_props_dialog_popup(C, op, 10 * UI_UNIT_X, 5 * UI_UNIT_Y);
		}
	}
	override_property_data.set = false;

	if (BLI_listbase_is_empty(&view_layer->override_sets)) {
		return view_layer_override_add_exec(C, op);
	}

	prop = RNA_struct_find_property(op->ptr, "override_set_index");
	if (RNA_property_is_set(op->ptr, prop)) {
		return view_layer_override_add_exec(C, op);
	}

	uiPopupMenu *pup;
	uiLayout *layout;

	/* Build the menus. */
	pup = UI_popup_menu_begin(C, IFACE_("Override Property in Set"), ICON_NONE);
	layout = UI_popup_menu_layout(pup);

	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

	int i = 0;
	for (OverrideSet *override_set = view_layer->override_sets.first; override_set; override_set = override_set->next) {
		uiItemIntO(layout,
		           override_set->name,
		           ICON_NONE,
		           "SCENE_OT_view_layer_override_add",
		           "override_set_index",
		           i++);
	}

	uiItemS(layout);

	uiItemBooleanO(layout,
	               "New Override Set",
	               ICON_ZOOMIN,
	               "SCENE_OT_view_layer_override_add",
	               "is_new",
	               true);

	UI_popup_menu_end(C, pup);

	return OPERATOR_INTERFACE;
}

static void SCENE_OT_view_layer_override_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add View Layer Override";
	ot->description = "Override property in a view layer override set";
	ot->idname = "SCENE_OT_view_layer_override_add";

	/* api callbacks */
	ot->exec = view_layer_override_add_exec;
	ot->invoke = view_layer_override_add_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_int(ot->srna, "override_set_index", 0, 0, INT_MAX,
	                   "Override Set Index", "Index of the override set to add the property", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
	prop = RNA_def_boolean(ot->srna, "is_new", false, "New", "Add a new override set");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
	prop = RNA_def_string(ot->srna, "new_override_set_name", "Override Set", MAX_NAME, "Name",
	                      "Name of the newly added override set");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int view_layer_override_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	OverrideSet *override_set = BLI_findlink(&view_layer->override_sets, view_layer->active_override_set);

	if (override_set == NULL) {
		return OPERATOR_CANCELLED;
	}

	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "index");
	if (!RNA_property_is_set(op->ptr, prop)) {
		BKE_report(op->reports, RPT_ERROR, "No property index defined");
		return OPERATOR_CANCELLED;
	}

	const int index = RNA_property_int_get(op->ptr, prop);

	prop = RNA_struct_find_property(op->ptr, "property_type");
	if (!RNA_property_is_set(op->ptr, prop)) {
		BKE_report(op->reports, RPT_ERROR, "No property type set");
		return OPERATOR_CANCELLED;
	}

	const int property_type = RNA_property_enum_get(op->ptr, prop);

	ListBase *lb[] = {
		&override_set->scene_properties,
		&override_set->collection_properties,
	};

	DynamicOverrideProperty *dyn_prop = BLI_findlink(lb[property_type], index);

	if (dyn_prop == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No property found");
		return OPERATOR_CANCELLED;
	}

	BKE_view_layer_override_property_remove(override_set, dyn_prop);

	DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE);
	WM_event_add_notifier(C, NC_SCENE | ND_DYN_OVERRIDES, scene);
	return OPERATOR_FINISHED;
}

static void SCENE_OT_view_layer_override_remove(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Remove View Layer Override";
	ot->description = "Remove override property in a view layer override set";
	ot->idname = "SCENE_OT_view_layer_override_remove";

	/* api callbacks */
	ot->exec = view_layer_override_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Property Index",
	            "Index of the property within its list", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
	prop = RNA_def_enum(ot->srna,
	             "property_type",
	             rna_enum_dynamic_override_property_type_items,
	             DYN_OVERRIDE_PROP_TYPE_SCENE,
	             "Property Type",
	             "Whether the property removed is a scene or a collection property");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

void ED_operatortypes_scene(void)
{
	WM_operatortype_append(SCENE_OT_new);
	WM_operatortype_append(SCENE_OT_delete);
	WM_operatortype_append(SCENE_OT_view_layer_override_set_add);
	WM_operatortype_append(SCENE_OT_view_layer_override_set_remove);
	WM_operatortype_append(SCENE_OT_override_set_collection_link);
	WM_operatortype_append(SCENE_OT_override_set_collection_unlink);
	WM_operatortype_append(SCENE_OT_view_layer_override_add);
	WM_operatortype_append(SCENE_OT_view_layer_override_remove);
}
