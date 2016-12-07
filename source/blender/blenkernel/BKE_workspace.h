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


/* -------------------------------------------------------------------- */
/* Create, delete, init */

struct WorkSpace *BKE_workspace_add(Main *bmain, const char *name);
void BKE_workspace_free(struct WorkSpace *ws);

struct WorkSpaceLayout *BKE_workspace_layout_add(struct WorkSpace *workspace, struct bScreen *screen) ATTR_NONNULL();
void BKE_workspace_layout_remove(struct WorkSpace *workspace, struct WorkSpaceLayout *layout, Main *bmain) ATTR_NONNULL();


/* -------------------------------------------------------------------- */
/* General Utils */

struct WorkSpaceLayout *BKE_workspace_layout_find(
        const struct WorkSpace *ws, const struct bScreen *screen) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
struct WorkSpaceLayout *BKE_workspace_layout_find_exec(
        const struct WorkSpace *ws, const struct bScreen *screen) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;


/* -------------------------------------------------------------------- */
/* Getters/Setters */

struct WorkSpaceLayout *BKE_workspace_active_layout_get(const struct WorkSpace *ws) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void                    BKE_workspace_active_layout_set(struct WorkSpace *ws, struct WorkSpaceLayout *layout) ATTR_NONNULL(1);
struct bScreen *BKE_workspace_active_screen_get(const struct WorkSpace *ws) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void            BKE_workspace_active_screen_set(struct WorkSpace *ws, struct bScreen *screen) ATTR_NONNULL(1);

struct bScreen *BKE_workspace_layout_screen_get(const struct WorkSpaceLayout *layout) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

#endif /* __BKE_WORKSPACE_H__ */
