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

/** \file blender/makesrna/intern/rna_workspace.c
 *  \ingroup RNA
 */

#include "BKE_workspace.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_types.h"

#include "rna_internal.h"


#ifdef RNA_RUNTIME

#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "WM_api.h"
#include "WM_types.h"


PointerRNA rna_workspace_screen_get(PointerRNA *ptr)
{
	WorkSpace *workspace = ptr->data;
	bScreen *screen = BKE_workspace_active_screen_get(workspace);

	return rna_pointer_inherit_refine(ptr, &RNA_Screen, screen);
}

static void rna_workspace_screen_set(PointerRNA *ptr, PointerRNA value)
{
	WorkSpace *ws = ptr->data;
	WorkSpaceLayout *layout_new;
	const bScreen *screen = BKE_workspace_active_screen_get(ws);

	/* disallow ID-browsing away from temp screens */
	if (screen->temp) {
		return;
	}
	if (value.data == NULL) {
		return;
	}

	/* exception: can't set screens inside of area/region handlers */
	layout_new = BKE_workspace_layout_find(ws, value.data);
	BKE_workspace_new_layout_set(ws, layout_new);
}

static int rna_workspace_screen_assign_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	bScreen *screen = value.id.data;
	return !screen->temp;
}

static void rna_workspace_screen_update(bContext *C, PointerRNA *ptr)
{
	WorkSpace *ws = ptr->data;
	WorkSpaceLayout *layout_new = BKE_workspace_new_layout_get(ws);

	/* exception: can't set screens inside of area/region handlers,
	 * and must use context so notifier gets to the right window */
	if (layout_new) {
		WM_event_add_notifier(C, NC_WORKSPACE | ND_SCREENBROWSE, layout_new);
		BKE_workspace_new_layout_set(ws, NULL);
	}
}

void rna_workspace_screens_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	WorkSpace *workspace = ptr->id.data;
	rna_iterator_listbase_begin(iter, BKE_workspace_layouts_get(workspace), NULL);
}

static PointerRNA rna_workspace_screens_item_get(CollectionPropertyIterator *iter)
{
	WorkSpaceLayout *layout = rna_iterator_listbase_get(iter);
	bScreen *screen = BKE_workspace_layout_screen_get(layout);

	return rna_pointer_inherit_refine(&iter->parent, &RNA_Screen, screen);
}

#ifdef USE_WORKSPACE_MODE

static int rna_workspace_object_mode_get(PointerRNA *ptr)
{
	WorkSpace *workspace = ptr->data;
	return (int)BKE_workspace_object_mode_get(workspace);
}

static void rna_workspace_object_mode_set(PointerRNA *ptr, int value)
{
	WorkSpace *workspace = ptr->data;
	BKE_workspace_object_mode_set(workspace, value);
}

#endif /* USE_WORKSPACE_MODE */

#else /* RNA_RUNTIME */

static void rna_def_workspace(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "WorkSpace", "ID");
	RNA_def_struct_sdna(srna, "WorkSpace");
	RNA_def_struct_ui_text(srna, "Workspace", "Workspace data-block, defining the working environment for the user");
	RNA_def_struct_ui_icon(srna, ICON_NONE);

	prop = RNA_def_property(srna, "screen", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "act_layout->screen");
	RNA_def_property_struct_type(prop, "Screen");
	RNA_def_property_ui_text(prop, "Screen", "Active screen showing in the workspace");
	RNA_def_property_pointer_funcs(prop, "rna_workspace_screen_get", "rna_workspace_screen_set", NULL,
	                               "rna_workspace_screen_assign_poll");
	RNA_def_property_flag(prop, PROP_NEVER_NULL | PROP_EDITABLE | PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_workspace_screen_update");

	prop = RNA_def_property(srna, "screens", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layouts", NULL);
	RNA_def_property_struct_type(prop, "Screen");
	RNA_def_property_collection_funcs(prop, "rna_workspace_screens_begin", NULL, NULL,
	                                  "rna_workspace_screens_item_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Screens", "Screen layouts of a workspace");

#ifdef USE_WORKSPACE_MODE
	prop = RNA_def_property(srna, "object_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_object_mode_items);
	RNA_def_property_enum_funcs(prop, "rna_workspace_object_mode_get", "rna_workspace_object_mode_set", NULL);
	RNA_def_property_ui_text(prop, "Mode", "Object interaction mode");
#endif
}

void RNA_def_workspace(BlenderRNA *brna)
{
	rna_def_workspace(brna);
}

#endif /* RNA_RUNTIME */
