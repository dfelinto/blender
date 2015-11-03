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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Benoit Bolsee
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file RAS_IOffScreen.h
 *  \ingroup bgerast
 */

#ifndef __RAS_OFFSCREEN_H__
#define __RAS_OFFSCREEN_H__

#include "EXP_Python.h"

class RAS_ICanvas;

class KX_Camera;
class KX_Scene;

class MT_Transform;

struct Image;

class RAS_IOffScreen
{
public:
	enum RAS_OFS_BIND_MODE {
		RAS_OFS_BIND_RENDER = 0,
		RAS_OFS_BIND_READ,
	};
	int		m_width;
	int     m_height;
	int	    m_samples;

	virtual ~RAS_IOffScreen() {}

	virtual bool Create(int width, int height, int samples) = 0;
	virtual void Destroy() = 0;
	virtual void Bind(RAS_OFS_BIND_MODE mode) = 0;
	virtual void Blit() = 0;
	virtual void Unbind() = 0;

	virtual int GetWidth() { return m_width; }
	virtual int GetHeight() { return m_height; }
	virtual int GetSamples() { return m_samples; }
};

#ifdef WITH_PYTHON
typedef struct {
	PyObject_HEAD
	RAS_IOffScreen *ofs;
} PyRASOffScreen;

extern PyTypeObject PyRASOffScreen_Type;
#endif

#endif  /* __RAS_OFFSCREEN_H__ */
