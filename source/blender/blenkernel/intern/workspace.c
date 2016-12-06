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

/** \file blender/blenkernel/intern/workspace.c
 *  \ingroup bke
 */

#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BLI_listbase.h"
#include "BKE_main.h"
#include "BKE_workspace.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"


static bool workspaces_is_screen_used(const Main *bmain, bScreen *screen);


/* -------------------------------------------------------------------- */
/* Create, delete, init */

WorkSpace *BKE_workspace_add(Main *bmain, const char *name)
{
	WorkSpace *new_ws = BKE_libblock_alloc(bmain, ID_WS, name);
	return new_ws;
}

void BKE_workspace_free(WorkSpace *ws)
{
	BLI_freelistN(&ws->layouts);
}


/**
 * Add a new layout to \a workspace for \a screen.
 */
WorkSpaceLayout *BKE_workspace_layout_add(WorkSpace *workspace, bScreen *screen)
{
	WorkSpaceLayout *layout = MEM_mallocN(sizeof(*layout), __func__);

	BLI_assert(!workspaces_is_screen_used(G.main, screen));
	layout->screen = screen;
	BLI_addhead(&workspace->layouts, layout);

	return layout;
}

void BKE_workspace_layout_remove(WorkSpace *workspace, WorkSpaceLayout *layout, Main *bmain)
{
	BKE_libblock_free(bmain, BKE_workspace_layout_screen_get(layout));
	BLI_remlink(&workspace->layouts, layout);
	MEM_freeN(layout);
}


/* -------------------------------------------------------------------- */
/* General Utils */

/**
 * Checks if \a screen is already used within any workspace. A screen should never be assigned to multiple
 * WorkSpaceLayouts, but that should be ensured outside of the BKE_workspace module and without such checks.
 * Hence, this should only be used as assert check before assigining a screen to a workflow.
 */
static bool workspaces_is_screen_used(const Main *bmain, bScreen *screen)
{
	for (WorkSpace *workspace = bmain->workspaces.first; workspace; workspace = workspace->id.next) {
		if (BKE_workspace_layout_find_exec(workspace, screen)) {
			return true;
		}
	}

	return false;
}

/**
 * This should only be used directly when it is to be expected that there isn't
 * a layout within \a workspace that wraps \a screen. Usually - especially outside
 * of BKE_workspace - #BKE_workspace_layout_find should be used!
 */
WorkSpaceLayout *BKE_workspace_layout_find_exec(const WorkSpace *ws, const bScreen *screen)
{
	for (WorkSpaceLayout *layout = ws->layouts.first; layout; layout = layout->next) {
		if (layout->screen == screen) {
			return layout;
		}
	}

	return NULL;
}

WorkSpaceLayout *BKE_workspace_layout_find(const WorkSpace *ws, const bScreen *screen)
{
	WorkSpaceLayout *layout = BKE_workspace_layout_find_exec(ws, screen);
	if (layout) {
		return layout;
	}

	BLI_assert(!"Couldn't find layout in this workspace. This should not happen!");
	return NULL;
}


/* -------------------------------------------------------------------- */
/* Getters/Setters */

WorkSpaceLayout *BKE_workspace_active_layout_get(const WorkSpace *ws)
{
	return ws->act_layout;
}
void BKE_workspace_active_layout_set(WorkSpace *ws, WorkSpaceLayout *layout)
{
	ws->act_layout = layout;
}

bScreen *BKE_workspace_active_screen_get(const WorkSpace *ws)
{
	return ws->act_layout->screen;
}
void BKE_workspace_active_screen_set(WorkSpace *ws, bScreen *screen)
{
	/* we need to find the WorkspaceLayout that wraps this screen */
	ws->act_layout = BKE_workspace_layout_find(ws, screen);
}

Scene *BKE_workspace_active_scene_get(const WorkSpace *ws)
{
	return ws->act_layout->screen->scene;
}
void BKE_workspace_active_scene_set(WorkSpace *ws, Scene *scene)
{
	ws->act_layout->screen->scene = scene;
}


bScreen *BKE_workspace_layout_screen_get(const WorkSpaceLayout *layout)
{
	return layout->screen;
}

Scene *BKE_workspace_layout_scene_get(const WorkSpaceLayout *layout)
{
	return layout->screen->scene;
}
