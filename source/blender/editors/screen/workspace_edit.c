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

/** \file blender/editors/screen/workspace_edit.c
 *  \ingroup edscr
 */

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_workspace.h"

#include "BLI_listbase.h"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"


/** \name Workspace API
 *
 * \brief API for managing workspaces and their data.
 * \{ */

static void workspace_unassign_window_workspace(wmWindow *win)
{
	win->workspace = NULL; /* just for safty, should always be set */
}

/**
 * \note Should either be called to initially assign workspace or after calling #workspace_unassign_window_workspace.
 */
static void workspace_assign_to_window(wmWindow *win, WorkSpace *ws)
{
	BLI_assert(win->workspace == NULL);
	win->workspace = ws;
}

void ED_workspace_change(wmWindow *win, WorkSpace *ws_new)
{
	workspace_unassign_window_workspace(win);
	workspace_assign_to_window(win, ws_new);
	/* TODO: send notifier */
}

/**
 * \return if succeeded.
 */
bool ED_workspace_delete(Main *bmain, wmWindow *win, WorkSpace *ws)
{
	if (BLI_listbase_is_single(&bmain->workspaces)) {
		return false;
	}

	if (win->workspace == ws) {
		WorkSpace *fallback_ws = ws->id.prev ? ws->id.prev : ws->id.next;
		ED_workspace_change(win, fallback_ws);
	}
	BKE_libblock_free(bmain, &ws->id);

	return true;
}

/** \} Workspace API */


/** \name Workspace Operators
 *
 * \{ */

static int workspace_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win = CTX_wm_window(C);
	WorkSpace *old_ws = win->workspace;
	WorkSpace *new_ws = BKE_workspace_duplicate(bmain, old_ws);

	ED_workspace_change(win, new_ws);

	return OPERATOR_FINISHED;
}

static void WORKSPACE_OT_workspace_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Workspace";
	ot->description = "Add a new workspace";
	ot->idname = "WORKSPACE_OT_workspace_new";

	/* api callbacks */
	ot->exec = workspace_new_exec;
	ot->poll = WM_operator_winactive;
}

static int workspace_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win = CTX_wm_window(C);

	ED_workspace_delete(bmain, win, win->workspace);

	return OPERATOR_FINISHED;
}

static void WORKSPACE_OT_workspace_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Workspace";
	ot->description = "Delete the active workspace";
	ot->idname = "WORKSPACE_OT_workspace_delete";

	/* api callbacks */
	ot->exec = workspace_delete_exec;
}

void ED_operatortypes_workspace(void)
{
	WM_operatortype_append(WORKSPACE_OT_workspace_new);
	WM_operatortype_append(WORKSPACE_OT_workspace_delete);
}

/** \} Workspace Operators */
