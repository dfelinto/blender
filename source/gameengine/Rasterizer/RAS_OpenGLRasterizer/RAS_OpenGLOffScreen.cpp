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
    :m_ofs(NULL), m_canvas(canvas), m_blitfbo(0), m_blittex(0), m_bound(false)
{
	m_width = 0;
	m_height = 0;
	m_samples = 0;
}

RAS_OpenGLOffScreen::~RAS_OpenGLOffScreen()
{
	Destroy();
}

bool RAS_OpenGLOffScreen::Create(int width, int height, int samples)
{
	char err_out[256];
	GLenum status;

	if (m_ofs)
	{
		printf("RAS_OpenGLOffScreen::Create(): buffer exists already, destroy first\n");
		return false;
	}

	m_ofs = GPU_offscreen_create(width, height, samples, err_out);
	if (m_ofs == NULL)
	{
		printf("RAS_OpenGLOffScreen::Create(): failed creating an offscreen buffer (%s)\n", err_out);
		return false;
	}
	m_width = width;
	m_height = height;

	if (GPU_offscreen_color_target(m_ofs) == GL_TEXTURE_2D_MULTISAMPLE)
	{
		GLuint blit_tex;
		GLuint blit_fbo;
		// create a secondary FBO to blit to before the pixel can be read

		/* create texture for new 'fbo_blit' */
		glGenTextures(1, &blit_tex);
		if (!blit_tex) {
			printf("RAS_OpenGLOffScreen::Create(): failed creating a texture for multi-sample offscreen buffer\n");
			goto L_ERROR;
		}
		m_blittex = blit_tex;
		glBindTexture(GL_TEXTURE_2D, m_blittex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		/* write into new single-sample buffer */
		glGenFramebuffersEXT(1, &blit_fbo);
		if (!blit_fbo) {
			printf("RAS_OpenGLOffScreen::Create(): failed creating a FBO for multi-sample offscreen buffer\n");
			goto L_ERROR;
		}
		m_blitfbo = blit_fbo;
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, m_blitfbo);
		glFramebufferTexture2DEXT(
		        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		        GL_TEXTURE_2D, m_blittex, 0);
		status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			goto L_ERROR;
		}
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		// remember that multisample is enabled
		m_samples = 1;
	}
	return true;

   L_ERROR:
	Destroy();
	return false;
}

void RAS_OpenGLOffScreen::Destroy()
{
	if (m_ofs)
	{
		Unbind();
		GPU_offscreen_free(m_ofs);
		m_ofs = NULL;
	}
	if (m_blittex)
	{
		GLuint tex = m_blittex;
		glDeleteTextures(1, &tex);
		m_blittex = 0;
	}
	if (m_blitfbo)
	{
		GLuint fbo = m_blitfbo;
		glDeleteFramebuffersEXT(1, &fbo);
		m_blitfbo = 0;
	}
	m_width = 0;
	m_height = 0;
	m_samples = 0;
}

void RAS_OpenGLOffScreen::Bind(RAS_OFS_BIND_MODE mode)
{
	if (m_ofs)
	{
		if (mode == RAS_OFS_BIND_RENDER || !m_blitfbo)
			GPU_offscreen_bind(m_ofs, false);
		else
			glBindFramebuffer(GL_READ_FRAMEBUFFER, m_blitfbo);
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

void RAS_OpenGLOffScreen::Blit()
{
	if (m_bound && m_blitfbo)
	{
		// set the draw target to the secondary FBO, the read target is still the multisample FBO
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_blitfbo);

		// sample the primary
		glBlitFramebufferEXT(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// make sur the next glReadPixels will read from the secondary buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_blitfbo);
	}
}
