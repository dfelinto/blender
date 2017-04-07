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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation, Dalai Felinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_legacy_stubs.h
 *  \ingroup gpu
 *
 *  This is to mark the transition to OpenGL core profile
 *  The idea is to allow Blender 2.8 to be built with OpenGL 3.3 even if it means breaking things
 *
 *  This file should be removed in the future
 */

#ifndef __GPU_LEGACY_STUBS_H__
#define __GPU_LEGACY_STUBS_H__

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "BLI_utildefines.h"

#define _GL_VOID static inline void
#define _GL_VOID_RET {}

#define _GL_INT static inline GLint
#define _GL_INT_RET { return 0; }

static bool disable_enable_check(GLenum cap)
{
	return ELEM(cap,
	            GL_ALPHA_TEST,
	            GL_LINE_STIPPLE,
	            GL_POINT_SPRITE,
	            GL_TEXTURE_1D,
	            GL_TEXTURE_2D,
	            GL_TEXTURE_GEN_S,
	            GL_TEXTURE_GEN_T,
	            -1
	            );
}

static bool tex_env_check(GLenum target, GLenum pname)
{
	return (ELEM(target, GL_TEXTURE_ENV) ||
	        (target == GL_TEXTURE_FILTER_CONTROL && pname == GL_TEXTURE_LOD_BIAS));
}

_GL_VOID DO_NOT_USE_glAlphaFunc (GLenum func, GLclampf ref) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glBegin (GLenum mode) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glClipPlane (GLenum plane, const GLdouble *equation) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glColor3f (GLfloat red, GLfloat green, GLfloat blue) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glColor3fv (const GLfloat *v) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glColor3ub (GLubyte red, GLubyte green, GLubyte blue) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glColor3ubv (const GLubyte *v) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glColor4ubv (const GLubyte *v) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glColorPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET

_GL_VOID USE_CAREFULLY_glDisable (GLenum cap)
{
	if (!disable_enable_check(cap)) {
		glDisable(cap);
	}
}
#define glDisable USE_CAREFULLY_glDisable

_GL_VOID DO_NOT_USE_glDisableClientState (GLenum array) _GL_VOID_RET

_GL_VOID USE_CAREFULLY_glEnable (GLenum cap)
{
	if (!disable_enable_check(cap)) {
		glEnable(cap);
	}
}
#define glEnable USE_CAREFULLY_glEnable

_GL_VOID DO_NOT_USE_glEnableClientState (GLenum array) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glEnd (void) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glInitNames (void) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glLightf (GLenum light, GLenum pname, GLfloat param) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glLightfv (GLenum light, GLenum pname, const GLfloat *params) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glLightModeli (GLenum pname, GLint param) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glLineStipple (GLint factor, GLushort pattern) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glLoadName (GLuint name) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glMaterialfv (GLenum face, GLenum pname, const GLfloat *params) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glMateriali (GLenum face, GLenum pname, GLint param) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glNormal3fv (const GLfloat *v) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glNormal3sv (const GLshort *v) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glNormalPointer (GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glPopName (void) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glPushName (GLuint name) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glRasterPos2f (GLfloat x, GLfloat y) _GL_VOID_RET

_GL_INT DO_NOT_USE_glRenderMode (GLenum mode) _GL_INT_RET

_GL_VOID DO_NOT_USE_glSelectBuffer (GLsizei size, GLuint *buffer) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glShadeModel (GLenum mode) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glTexCoord2fv (const GLfloat *v) _GL_VOID_RET

#if 0
_GL_VOID USE_CAREFULLY_glTexEnvf(GLenum target, GLenum pname, GLint param)
{
	if (!tex_env_check(target, pname)) {
		glTexEnvf(target, pname, param);
	}
}
#define glTexEnvf USE_CAREFULLY_glTexEnvf

_GL_VOID USE_CAREFULLY_glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	if (!tex_env_check(target, pname)) {
		glTexEnvfv(target, pname, params);
	}
}
#define glTexEnvfv USE_CAREFULLY_glTexEnvfv

_GL_VOID USE_CAREFULLY_glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	if (!tex_env_check(target, pname)) {
		glTexEnvi(target, pname, param);
	}
}
#define glTexEnvi USE_CAREFULLY_glTexEnvi

_GL_VOID USE_CAREFULLY_glTexGeni(GLenum coord, GLenum pname, GLint param)
{
	if (pname != GL_TEXTURE_GEN_MODE) {
		glTexGeni(coord, pname, param);
	}
}
#define glTexGeni USE_CAREFULLY_glTexGeni
#endif

_GL_VOID DO_NOT_USE_glVertex2f (GLfloat x, GLfloat y) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glVertex3f (GLfloat x, GLfloat y, GLfloat z) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glTexCoord3fv (const GLfloat *v) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glVertexPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET

_GL_VOID DO_NOT_USE_glVertex3fv (const GLfloat *v) _GL_VOID_RET


#undef _GL_VOID
#undef _GL_VOID_RET

#undef _GL_INT
#undef _GL_INT_RET


#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#endif /* __GPU_LEGACY_STUBS_H__ */
