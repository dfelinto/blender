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

#include "screen_intern.h"


/** \name Workspace API
 *
 * \brief API for managing workspaces and their data.
 * \{ */


/**
 * \brief Change the active workspace.
 *
 * Operator call, WM + Window + screen already existed before
 * Pretty similar to #ED_screen_set since changing workspace also changes screen.
 *
 * \warning Do NOT call in area/region queues!
 * \returns success.
 */
bool ED_workspace_change(bContext *C, wmWindow *win, WorkSpace *ws_new)
{
	Main *bmain = CTX_data_main(C);
	bScreen *screen_old = BKE_workspace_active_screen_get(win->workspace);
	bScreen *screen_new = BKE_workspace_active_screen_get(ws_new);

	if (!(screen_new = screen_set_ensure_valid(bmain, win, screen_new))) {
		return false;
	}

	if (screen_old != screen_new) {
		screen_set_prepare(C, win, screen_new, screen_old);
		win->workspace = ws_new;
		screen_set_refresh(bmain, C, win, screen_old->scene != screen_new->scene);
	}
	BLI_assert(CTX_wm_workspace(C) == ws_new);

	return true;
}

/**
 * Duplicate a workspace including its active screen (since two workspaces can't show the same screen).
 */
WorkSpace *ED_workspace_duplicate(Main *bmain, wmWindow *win)
{
	bScreen *old_screen = WM_window_get_active_screen(win);
	bScreen *new_screen = ED_screen_duplicate(win, old_screen, NULL);
	WorkSpace *old_ws = win->workspace;

	new_screen->winid = win->winid;
	new_screen->do_refresh = true;
	new_screen->do_draw = true;

	return BKE_workspace_duplicate(bmain, old_ws, new_screen);
}

/**
 * \return if succeeded.
 */
bool ED_workspace_delete(Main *bmain, bContext *C, wmWindow *win, WorkSpace *ws)
{
	if (BLI_listbase_is_single(&bmain->workspaces)) {
		return false;
	}

	if (win->workspace == ws) {
		WorkSpace *fallback_ws = ws->id.prev ? ws->id.prev : ws->id.next;
		ED_workspace_change(C, win, fallback_ws);
	}
	BKE_libblock_free(bmain, &ws->id);

	return true;
}

static bool workspace_layout_set_poll(const WorkSpaceLayout *layout)
{
	const bScreen *screen = layout->screen;

	return ((screen->winid == 0) &&
	        /* in typical usage these should have a nonzero winid
	         * (all temp screens should be used, or closed & freed). */
	        (screen->temp == false) &&
	        (screen->state == SCREENNORMAL) &&
	        (screen->id.name[2] != '.' || !(U.uiflag & USER_HIDE_DOT)));
}

bool ED_workspace_layout_circle(bContext *C, wmWindow *win, const short direction)
{
	const WorkSpace *workspace = win->workspace;
	WorkSpaceLayout *old_layout = WM_window_get_active_layout(win);
	WorkSpaceLayout *new_layout;
	ScrArea *sa = CTX_wm_area(C);

	if (WM_window_is_temp_screen(win) || (sa && sa->full && sa->full->temp)) {
		return false;
	}

	if (direction == 1) {
		BLI_LISTBASE_CIRCULAR_FORWARD_BEGIN(&workspace->layouts, new_layout, old_layout)
		{
			if (workspace_layout_set_poll(new_layout)) {
				break;
			}
		}
		BLI_LISTBASE_CIRCULAR_FORWARD_END(&workspace->layouts, new_layout, old_layout)
	}
	else if (direction == -1) {
		BLI_LISTBASE_CIRCULAR_BACKWARD_BEGIN(&workspace->layouts, new_layout, old_layout)
		{
			if (workspace_layout_set_poll(new_layout)) {
				break;
			}
		}
		BLI_LISTBASE_CIRCULAR_BACKWARD_END(&workspace->layouts, new_layout, old_layout)
	}
	else {
		BLI_assert(0);
	}

	if (new_layout && (old_layout != new_layout)) {
		if (sa && sa->full) {
			/* return to previous state before switching screens */
			ED_screen_full_restore(C, sa); /* may free screen of old_layout */
		}

		ED_screen_set(C, new_layout->screen);

		return true;
	}

	return false;
}

/** \} Workspace API */


/** \name Workspace Operators
 *
 * \{ */

static int workspace_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win = CTX_wm_window(C);
	WorkSpace *workspace;

	workspace = ED_workspace_duplicate(bmain, win);
	WM_event_add_notifier(C, NC_SCREEN | ND_WORKSPACE_SET, workspace);

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

	ED_workspace_delete(bmain, C, win, win->workspace);

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
