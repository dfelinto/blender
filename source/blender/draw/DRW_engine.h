/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file DRW_engine.h
 *  \ingroup draw
 */

#ifndef __DRW_ENGINE_H__
#define __DRW_ENGINE_H__

typedef enum {
	DRW_UNIFORM_INT,
	DRW_UNIFORM_FLOAT,
	DRW_UNIFORM_TEXTURE,
	DRW_UNIFORM_BUFFER,
	DRW_UNIFORM_MAT3,
	DRW_UNIFORM_MAT4
} DRWUniformType;

typedef struct DRWUniform {
	struct DRWUniform *next, *prev;
	DRWUniformType type;
	int location;
	int length;
	int arraysize;
	int bindloc;
	const void *value;
} DRWUniform;

typedef struct DRWInterface {
	ListBase uniforms;
	/* matrices locations */
	int modelview;
	int projection;
	int modelviewprojection;
	int normal;
} DRWInterface;

void DRW_viewport_engine_init(void);

#endif /* __DRW_ENGINE_H__ */