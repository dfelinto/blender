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

#define NAMESPACE_WORKSPACE /* allow including specially guarded dna_workspace_types.h */

#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BLI_listbase.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "dna_workspace_types.h"

#include "MEM_guardedalloc.h"


static bool workspaces_is_screen_used(const Main *bmain, bScreen *screen);


/* -------------------------------------------------------------------- */
/* Create, delete, init */

/**
 * Only to be called by #BKE_libblock_alloc_notest! Always use BKE_workspace_add to add a new workspace.
 */
WorkSpace *workspace_alloc(void)
{
	return MEM_callocN(sizeof(WorkSpace), "Workspace");
}

WorkSpace *BKE_workspace_add(Main *bmain, const char *name)
{
	WorkSpace *new_ws = BKE_libblock_alloc(bmain, ID_WS, name);
	return new_ws;
}

void BKE_workspace_free(WorkSpace *ws)
{
	BLI_freelistN(&ws->layouts);
}

void BKE_workspace_remove(WorkSpace *workspace, Main *bmain)
{
	BKE_workspace_layout_iter_begin(layout, workspace->layouts.first)
	{
		BKE_workspace_layout_remove(workspace, layout, bmain);
	}
	BKE_workspace_layout_iter_end;

	BKE_libblock_free(bmain, workspace);
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
	BLI_freelinkN(&workspace->layouts, layout);
}


/* -------------------------------------------------------------------- */
/* General Utils */

void BKE_workspaces_transform_orientation_remove(const ListBase *workspaces, const TransformOrientation *orientation)
{
	BKE_workspace_iter_begin(workspace, workspaces->first)
	{
		BKE_workspace_layout_iter_begin(layout, workspace->layouts.first)
		{
			BKE_screen_transform_orientation_remove(BKE_workspace_layout_screen_get(layout), orientation);
		}
		BKE_workspace_layout_iter_end;
	}
	BKE_workspace_iter_end;
}

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

WorkSpaceLayout *BKE_workspace_layout_iter_circular(const WorkSpace *workspace, WorkSpaceLayout *start,
                                                    bool (*callback)(const WorkSpaceLayout *layout, void *arg),
                                                    void *arg, const bool iter_backward)
{
	WorkSpaceLayout *iter_layout;

	if (iter_backward) {
		BLI_LISTBASE_CIRCULAR_BACKWARD_BEGIN(&workspace->layouts, iter_layout, start)
		{
			if (!callback(iter_layout, arg)) {
				return iter_layout;
			}
		}
		BLI_LISTBASE_CIRCULAR_BACKWARD_END(&workspace->layouts, iter_layout, start);
	}
	else {
		BLI_LISTBASE_CIRCULAR_FORWARD_BEGIN(&workspace->layouts, iter_layout, start)
		{
			if (!callback(iter_layout, arg)) {
				return iter_layout;
			}
		}
		BLI_LISTBASE_CIRCULAR_FORWARD_END(&workspace->layouts, iter_layout, start)
	}

	return NULL;
}


/* -------------------------------------------------------------------- */
/* Getters/Setters */

ID *BKE_workspace_id_get(WorkSpace *workspace)
{
	return &workspace->id;
}

const char *BKE_workspace_name_get(const WorkSpace *workspace)
{
	return workspace->id.name + 2;
}

WorkSpaceLayout *BKE_workspace_active_layout_get(const WorkSpace *workspace)
{
	return workspace->act_layout;
}
void BKE_workspace_active_layout_set(WorkSpace *workspace, WorkSpaceLayout *layout)
{
	workspace->act_layout = layout;
}

WorkSpaceLayout *BKE_workspace_new_layout_get(const WorkSpace *workspace)
{
	return workspace->new_layout;
}
void BKE_workspace_new_layout_set(WorkSpace *workspace, WorkSpaceLayout *layout)
{
	workspace->new_layout = layout;
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

ObjectMode BKE_workspace_object_mode_get(const WorkSpace *workspace)
{
	return workspace->object_mode;
}
void BKE_workspace_object_mode_set(WorkSpace *workspace, const ObjectMode mode)
{
	workspace->object_mode = mode;
}

ListBase *BKE_workspace_layouts_get(WorkSpace *workspace)
{
	return &workspace->layouts;
}

WorkSpace *BKE_workspace_next_get(const WorkSpace *workspace)
{
	return workspace->id.next;
}
WorkSpace *BKE_workspace_prev_get(const WorkSpace *workspace)
{
	return workspace->id.prev;
}


bScreen *BKE_workspace_layout_screen_get(const WorkSpaceLayout *layout)
{
	return layout->screen;
}
void BKE_workspace_layout_screen_set(WorkSpaceLayout *layout, bScreen *screen)
{
	layout->screen = screen;
}

WorkSpaceLayout *BKE_workspace_layout_next_get(const WorkSpaceLayout *layout)
{
	return layout->next;
}
WorkSpaceLayout *BKE_workspace_layout_prev_get(const WorkSpaceLayout *layout)
{
	return layout->prev;
}
