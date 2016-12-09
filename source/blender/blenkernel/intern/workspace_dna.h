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

/** \file blender/blenkernel/intern/workspace_dna.h
 *  \ingroup bke
 *
 * Local header with WorkSpace DNA types. makesdna.c includes this.
 */

#ifndef __WORKSPACE_DNA_H__
#define __WORKSPACE_DNA_H__

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
} WorkSpace;

#endif /* __WORKSPACE_DNA_H__ */
