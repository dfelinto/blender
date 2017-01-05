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

/** \file blender/editors/screen/workspace_layout_edit.c
 *  \ingroup edscr
 */

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_screen_types.h"

#include "ED_screen.h"

#include "WM_api.h"

#include "screen_intern.h"


/**
 * Empty screen, with 1 dummy area without spacedata. Uses window size.
 */
WorkSpaceLayout *ED_workspace_layout_add(WorkSpace *workspace, wmWindow *win, const char *name)
{
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	bScreen *screen = screen_add(win, name, winsize_x, winsize_y);
	WorkSpaceLayout *layout = BKE_workspace_layout_add(workspace, screen);

	return layout;
}

WorkSpaceLayout *ED_workspace_layout_duplicate(WorkSpace *workspace, const WorkSpaceLayout *layout_old, wmWindow *win)
{
	bScreen *screen_old = BKE_workspace_layout_screen_get(layout_old);
	bScreen *screen_new;
	WorkSpaceLayout *layout_new;

	if (BKE_screen_is_fullscreen_area(screen_old)) {
		return NULL; /* XXX handle this case! */
	}

	layout_new = ED_workspace_layout_add(workspace, win, screen_old->id.name + 2);
	screen_new = BKE_workspace_layout_screen_get(layout_new);
	screen_data_copy(screen_new, screen_old);

	return layout_new;
}

static bool workspace_layout_delete_doit(bContext *C, WorkSpace *workspace,
                                         WorkSpaceLayout *layout_old, WorkSpaceLayout *layout_new)
{
	Main *bmain = CTX_data_main(C);
	bScreen *screen_new = BKE_workspace_layout_screen_get(layout_new);

	ED_screen_change(C, screen_new);

	if (BKE_workspace_active_layout_get(workspace) != layout_old) {
		BKE_workspace_layout_remove(workspace, layout_old, bmain);
		return true;
	}

	return false;
}

static bool workspace_layout_set_poll(const WorkSpaceLayout *layout)
{
	const bScreen *screen = BKE_workspace_layout_screen_get(layout);

	return ((BKE_screen_is_used(screen) == false) &&
	        /* in typical usage temp screens should have a nonzero winid
	         * (all temp screens should be used, or closed & freed). */
	        (screen->temp == false) &
	        (BKE_screen_is_fullscreen_area(screen) == false) &&
	        (screen->id.name[2] != '.' || !(U.uiflag & USER_HIDE_DOT)));
}

static WorkSpaceLayout *workspace_layout_delete_find_new(const WorkSpaceLayout *layout_old)
{
	WorkSpaceLayout *prev = BKE_workspace_layout_prev_get(layout_old);
	WorkSpaceLayout *next = BKE_workspace_layout_next_get(layout_old);

	BKE_workspace_layout_iter_backwards_begin(layout_new, prev)
	{
		if (workspace_layout_set_poll(layout_new)) {
			return layout_new;
		}
	}
	BKE_workspace_layout_iter_end;
	BKE_workspace_layout_iter_begin(layout_new, next)
	{
		if (workspace_layout_set_poll(layout_new)) {
			return layout_new;
		}
	}
	BKE_workspace_layout_iter_end;

	return NULL;
}

/**
 * \warning Only call outside of area/region loops!
 * \return true if succeeded.
 */
bool ED_workspace_layout_delete(bContext *C, WorkSpace *workspace, WorkSpaceLayout *layout_old)
{
	const bScreen *screen_old = BKE_workspace_layout_screen_get(layout_old);
	WorkSpaceLayout *layout_new;

	BLI_assert(BLI_findindex(BKE_workspace_layouts_get(workspace), layout_old) != -1);

	/* don't allow deleting temp fullscreens for now */
	if (BKE_screen_is_fullscreen_area(screen_old)) {
		return false;
	}

	/* A layout/screen can only be in use by one window at a time, so as
	 * long as we are able to find a layout/screen that is unused, we
	 * can safely assume ours is not in use anywhere an delete it. */

	layout_new = workspace_layout_delete_find_new(layout_old);

	if (layout_new) {
		return workspace_layout_delete_doit(C, workspace, layout_old, layout_new);
	}

	return false;
}

static bool workspace_layout_cycle_iter_cb(const WorkSpaceLayout *layout, void *UNUSED(arg))
{
	return workspace_layout_set_poll(layout);
}

bool ED_workspace_layout_cycle(bContext *C, WorkSpace *workspace, const short direction)
{
	WorkSpaceLayout *old_layout = BKE_workspace_active_layout_get(workspace);
	WorkSpaceLayout *new_layout;
	const bScreen *old_screen = BKE_workspace_layout_screen_get(old_layout);
	ScrArea *sa = CTX_wm_area(C);

	if (old_screen->temp || (sa && sa->full && sa->full->temp)) {
		return false;
	}

	BLI_assert(ELEM(direction, 1, -1));
	new_layout = BKE_workspace_layout_iter_circular(workspace, old_layout, workspace_layout_cycle_iter_cb,
	                                                NULL, (direction == -1) ? true : false);

	if (new_layout && (old_layout != new_layout)) {
		bScreen *new_screen = BKE_workspace_layout_screen_get(new_layout);

		if (sa && sa->full) {
			/* return to previous state before switching screens */
			ED_screen_full_restore(C, sa); /* may free screen of old_layout */
		}

		ED_screen_change(C, new_screen);

		return true;
	}

	return false;
}
