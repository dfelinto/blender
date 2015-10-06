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

#include "glew-mx.h"

#include <stdio.h>

#include "RAS_OpenGLOffScreen.h"
#include "RAS_ICanvas.h"

RAS_OpenGLOffScreen::RAS_OpenGLOffScreen(RAS_ICanvas *canvas)
    :m_ofs(NULL), m_canvas(canvas), m_bound(false)
{
	m_width = 0;
	m_height = 0;
}

RAS_OpenGLOffScreen::~RAS_OpenGLOffScreen()
{
	Destroy();
}

bool RAS_OpenGLOffScreen::Create(int width, int height)
{
	char err_out[256];

	if (m_ofs)
	{
		printf("RAS_OpenGLOffScreen::Create(): buffer exists already, destroy first\n");
		return false;
	}

	m_ofs = GPU_offscreen_create(width, height, err_out);
	if (m_ofs == NULL)
	{
		printf("RAS_OpenGLOffScreen::Create(): failed creating an offscreen buffer (%s)\n", err_out);
		return false;
	}
	m_width = width;
	m_height = height;
	return true;
}

void RAS_OpenGLOffScreen::Destroy()
{
	if (m_ofs)
	{
		Unbind();
		GPU_offscreen_free(m_ofs);
		m_ofs = NULL;
	}
	m_width = 0;
	m_height = 0;
}

void RAS_OpenGLOffScreen::Bind()
{
	if (m_ofs)
	{
		GPU_offscreen_bind(m_ofs, false);
		m_bound = true;
	}
}

void RAS_OpenGLOffScreen::Unbind()
{
	if (m_ofs && m_bound)
	{
		GPU_offscreen_unbind(m_ofs, false);
		m_bound = false;
	}
}

