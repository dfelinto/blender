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

/** \file DNA_workspace_types.h
 *  \ingroup DNA
 *
 * Only use with API in BKE_workspace.h!
 */

#ifndef __DNA_WORKSPACE_TYPES_H__
#define __DNA_WORKSPACE_TYPES_H__

#if !defined(NAMESPACE_WORKSPACE) && !defined(NAMESPACE_DNA)
#  error "This file shouldn't be included outside of workspace namespace."
#endif

/**
 * Layouts are basically bScreens. We use this struct to wrap a reference to a screen so that we can store it in
 * a ListBase within a workspace. Usually you shouldn't have to deal with it, only with bScreen and WorkSpace.
 */
typedef struct WorkSpaceLayout {
	struct WorkSpaceLayout *next, *prev;

	struct bScreen *screen;
} WorkSpaceLayout;

typedef struct WorkSpace {
	ID id;

	ListBase layouts;
	struct WorkSpaceLayout *act_layout;
	struct WorkSpaceLayout *new_layout; /* temporary when switching screens */

	int object_mode; /* enum ObjectMode */
	int pad;
} WorkSpace;

#endif /* __DNA_WORKSPACE_TYPES_H__ */
