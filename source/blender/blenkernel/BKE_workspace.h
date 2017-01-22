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

/** \file BKE_workspace.h
 *  \ingroup bke
 */

#ifndef __BKE_WORKSPACE_H__
#define __BKE_WORKSPACE_H__

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

struct bScreen;
struct ListBase;
struct Main;
struct TransformOrientation;
struct WorkSpace;

typedef struct WorkSpace WorkSpace;
typedef struct WorkSpaceLayout WorkSpaceLayout;

/**
 * Plan is to store the object-mode per workspace, not per object anymore.
 * However, there's quite some work to be done for that, so for now, there is just a basic
 * implementation of an object <-> workspace object-mode syncing for testing, with some known
 * problems. Main problem being that the modes can get out of sync when changing object selection.
 * Would require a pile of temporary changes to always sync modes when changing selection. So just
 * leaving this here for some testing until object-mode is really a workspace level setting.
 */
#define USE_WORKSPACE_MODE


/* -------------------------------------------------------------------- */
/* Create, delete, init */

WorkSpace *BKE_workspace_add(struct Main *bmain, const char *name);
void BKE_workspace_free(WorkSpace *ws);
void BKE_workspace_remove(WorkSpace *workspace, struct Main *bmain);

struct WorkSpaceLayout *BKE_workspace_layout_add(WorkSpace *workspace, struct bScreen *screen) ATTR_NONNULL();
void BKE_workspace_layout_remove(WorkSpace *workspace, WorkSpaceLayout *layout, struct Main *bmain) ATTR_NONNULL();


/* -------------------------------------------------------------------- */
/* General Utils */

#define BKE_workspace_iter_begin(_workspace, _start_workspace) \
	for (WorkSpace *_workspace = _start_workspace, *_workspace##_next; _workspace; _workspace = _workspace##_next) { \
		_workspace##_next = BKE_workspace_next_get(_workspace); /* support removing workspace from list */
#define BKE_workspace_iter_end } (void)0

void BKE_workspaces_transform_orientation_remove(const struct ListBase *workspaces,
                                                 const struct TransformOrientation *orientation) ATTR_NONNULL();

WorkSpaceLayout *BKE_workspace_layout_find(const WorkSpace *ws, const struct bScreen *screen) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
WorkSpaceLayout *BKE_workspace_layout_find_exec(const WorkSpace *ws, const struct bScreen *screen) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

#define BKE_workspace_layout_iter_begin(_layout, _start_layout) \
	for (WorkSpaceLayout *_layout = _start_layout, *_layout##_next; _layout; _layout = _layout##_next) { \
		_layout##_next = BKE_workspace_layout_next_get(_layout); /* support removing layout from list */
#define BKE_workspace_layout_iter_backwards_begin(_layout, _start_layout) \
	for (WorkSpaceLayout *_layout = _start_layout, *_layout##_prev; _layout; _layout = _layout##_prev) {\
		_layout##_prev = BKE_workspace_layout_prev_get(_layout); /* support removing layout from list */
#define BKE_workspace_layout_iter_end } (void)0

WorkSpaceLayout *BKE_workspace_layout_iter_circular(const WorkSpace *workspace, WorkSpaceLayout *start,
                                                    bool (*callback)(const WorkSpaceLayout *layout, void *arg),
                                                    void *arg, const bool iter_backward);


/* -------------------------------------------------------------------- */
/* Getters/Setters */

struct ID *BKE_workspace_id_get(WorkSpace *workspace);
const char *BKE_workspace_name_get(const WorkSpace *workspace);
WorkSpaceLayout *BKE_workspace_active_layout_get(const struct WorkSpace *ws) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void             BKE_workspace_active_layout_set(WorkSpace *ws, WorkSpaceLayout *layout) ATTR_NONNULL(1);
struct bScreen *BKE_workspace_active_screen_get(const WorkSpace *ws) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void            BKE_workspace_active_screen_set(WorkSpace *ws, struct bScreen *screen) ATTR_NONNULL(1);
enum ObjectMode BKE_workspace_object_mode_get(const WorkSpace *workspace) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
#ifdef USE_WORKSPACE_MODE
void            BKE_workspace_object_mode_set(WorkSpace *workspace, const enum ObjectMode mode) ATTR_NONNULL();
#endif
struct ListBase *BKE_workspace_layouts_get(WorkSpace *workspace) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
WorkSpaceLayout *BKE_workspace_new_layout_get(const WorkSpace *workspace) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void             BKE_workspace_new_layout_set(WorkSpace *workspace, WorkSpaceLayout *layout) ATTR_NONNULL(1);

WorkSpace *BKE_workspace_next_get(const WorkSpace *workspace) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
WorkSpace *BKE_workspace_prev_get(const WorkSpace *workspace) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

struct bScreen *BKE_workspace_layout_screen_get(const WorkSpaceLayout *layout) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void            BKE_workspace_layout_screen_set(WorkSpaceLayout *layout, struct bScreen *screen) ATTR_NONNULL(1);

WorkSpaceLayout *BKE_workspace_layout_next_get(const WorkSpaceLayout *layout) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
WorkSpaceLayout *BKE_workspace_layout_prev_get(const WorkSpaceLayout *layout) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

/* -------------------------------------------------------------------- */
/* Don't use outside of BKE! */

WorkSpace *workspace_alloc(void) ATTR_WARN_UNUSED_RESULT;

#endif /* __BKE_WORKSPACE_H__ */
