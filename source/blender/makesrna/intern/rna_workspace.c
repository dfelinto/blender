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

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"


#ifdef RNA_RUNTIME

#include "BKE_workspace.h"

#include "DNA_screen_types.h"

#include "WM_api.h"
#include "WM_types.h"


static void rna_workspace_screen_set(PointerRNA *ptr, PointerRNA value)
{
	WorkSpace *ws = (WorkSpace *)ptr->data;
	const bScreen *screen = BKE_workspace_active_screen_get(ws);

	/* disallow ID-browsing away from temp screens */
	if (screen->temp) {
		return;
	}
	if (value.data == NULL) {
		return;
	}

	/* exception: can't set screens inside of area/region handlers */
	ws->new_layout = BKE_workspace_layout_find(ws, value.data);
}

static int rna_workspace_screen_assign_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	bScreen *screen = (bScreen *)value.id.data;
	return !screen->temp;
}

static void rna_workspace_screen_update(bContext *C, PointerRNA *ptr)
{
	WorkSpace *ws = (WorkSpace *)ptr->data;

	/* exception: can't set screens inside of area/region handlers,
	 * and must use context so notifier gets to the right window */
	if (ws->new_layout) {
		WM_event_add_notifier(C, NC_SCREEN | ND_SCREENBROWSE, ws->new_layout);
		ws->new_layout = NULL;
	}
}

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
	RNA_def_property_pointer_funcs(prop, NULL, "rna_workspace_screen_set", NULL, "rna_workspace_screen_assign_poll");
	RNA_def_property_flag(prop, PROP_NEVER_NULL | PROP_EDITABLE | PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_workspace_screen_update");
}

void RNA_def_workspace(BlenderRNA *brna)
{
	rna_def_workspace(brna);
}

#endif /* RNA_RUNTIME */
