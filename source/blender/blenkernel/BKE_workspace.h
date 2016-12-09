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

struct bScreen;
struct WorkSpace;

typedef struct WorkSpace WorkSpace;
typedef struct WorkSpaceLayout WorkSpaceLayout;


/* -------------------------------------------------------------------- */
/* Create, delete, init */

WorkSpace *BKE_workspace_add(Main *bmain, const char *name);
void BKE_workspace_free(WorkSpace *ws);

struct WorkSpaceLayout *BKE_workspace_layout_add(WorkSpace *workspace, struct bScreen *screen) ATTR_NONNULL();
void BKE_workspace_layout_remove(WorkSpace *workspace, WorkSpaceLayout *layout, Main *bmain) ATTR_NONNULL();


/* -------------------------------------------------------------------- */
/* General Utils */

#define BKE_workspace_iter(_workspace, _start_workspace) \
	for (WorkSpace *_workspace = _start_workspace; _workspace; _workspace = BKE_workspace_next_get(_workspace))

WorkSpaceLayout *BKE_workspace_layout_find(const WorkSpace *ws, const struct bScreen *screen) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
WorkSpaceLayout *BKE_workspace_layout_find_exec(const WorkSpace *ws, const struct bScreen *screen) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

#define BKE_workspace_layout_iter(_layout, _start_layout) \
	for (WorkSpaceLayout *_layout = _start_layout; _layout; _layout = BKE_workspace_layout_next_get(_layout))
#define BKE_workspace_layout_iter_backwards(_layout, _start_layout) \
	for (WorkSpaceLayout *_layout = _start_layout; _layout; _layout = BKE_workspace_layout_prev_get(_layout))

WorkSpaceLayout *BKE_workspace_layout_iter_circular(const WorkSpace *workspace, WorkSpaceLayout *start,
                                                    bool (*callback)(const WorkSpaceLayout *layout, void *arg),
                                                    void *arg, const bool iter_backward);


/* -------------------------------------------------------------------- */
/* Getters/Setters */

struct ID *BKE_workspace_id_get(WorkSpace *workspace);
const char *BKE_workspace_name_get(const WorkSpace *workspace);
WorkSpaceLayout *BKE_workspace_active_layout_get(const struct WorkSpace *ws) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void             BKE_workspace_active_layout_set(WorkSpace *ws, WorkSpaceLayout *layout) ATTR_NONNULL(1);
struct bScreen *BKE_workspace_active_screen_get(const struct WorkSpace *ws) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void            BKE_workspace_active_screen_set(WorkSpace *ws, struct bScreen *screen) ATTR_NONNULL(1);
ListBase *BKE_workspace_layouts_get(WorkSpace *workspace) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
WorkSpaceLayout *BKE_workspace_new_layout_get(const WorkSpace *workspace) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void             BKE_workspace_new_layout_set(WorkSpace *workspace, WorkSpaceLayout *layout) ATTR_NONNULL(1);

WorkSpace *BKE_workspace_next_get(const WorkSpace *workspace) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
WorkSpace *BKE_workspace_prev_get(const WorkSpace *workspace) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

struct bScreen *BKE_workspace_layout_screen_get(const WorkSpaceLayout *layout) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void            BKE_workspace_layout_screen_set(WorkSpaceLayout *layout, struct bScreen *screen) ATTR_NONNULL(1);

WorkSpaceLayout *BKE_workspace_layout_next_get(const WorkSpaceLayout *layout) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
WorkSpaceLayout *BKE_workspace_layout_prev_get(const WorkSpaceLayout *layout) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

#endif /* __BKE_WORKSPACE_H__ */
