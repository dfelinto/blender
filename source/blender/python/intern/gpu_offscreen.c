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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/gpu_offscreen.c
 *  \ingroup pythonintern
 *
 * This file defines the offscreen functionalities of the 'gpu' module
 * used for offline rendering
 */

/* python redefines */
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#include <Python.h>

#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "WM_types.h"

#include "ED_screen.h"

#include "GPU_extensions.h"
#include "GPU_compositing.h"

#include "../mathutils/mathutils.h"

#include "bpy_util.h"

#include "../generic/py_capi_utils.h"

#include "gpu.h"

/* -------------------------------------------------------------------- */
/* GPU Offscreen PyObject */

typedef struct {
	PyObject_HEAD
	GPUOffScreen *ofs;
} PyGPUOffScreen;

static int bpy_gpu_offscreen_valid_check(PyGPUOffScreen *pygpu, const char *error_prefix)
{
	if (UNLIKELY(pygpu->ofs == NULL)) {
		PyErr_Format(PyExc_ReferenceError,
		             "%.200s: GPU offscreen was freed, no further access is valid",
		             error_prefix, Py_TYPE(pygpu)->tp_name);
		return -1;
	}
	return 0;
}

#define BPY_GPU_OFFSCREEN_CHECK_OBJ(pygpu, errmsg) { \
    if (UNLIKELY(bpy_gpu_offscreen_valid_check(pygpu, errmsg) == -1)) { \
		return NULL; \
	} \
} (void)0

/* annoying since arg parsing won't check overflow */
#define UINT_IS_NEG(n) ((n) > INT_MAX)

PyDoc_STRVAR(pygpu_offscreen_width_doc, "Texture width.\n\n:type: GLsizei");
static PyObject *pygpu_offscreen_width_get(PyGPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self, "width");
	return PyLong_FromLong(GPU_offscreen_width(self->ofs));
}

PyDoc_STRVAR(pygpu_offscreen_height_doc, "Texture height.\n\n:type: GLsizei");
static PyObject *pygpu_offscreen_height_get(PyGPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self, "height");
	return PyLong_FromLong(GPU_offscreen_height(self->ofs));
}

PyDoc_STRVAR(pygpu_offscreen_framebuffer_object_doc, "Framebuffer object.\n\n:type: GLuint");
static PyObject *pygpu_offscreen_framebuffer_object_get(PyGPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self, "framebuffer object");
	return PyLong_FromLong(GPU_offscreen_fb_object(self->ofs));
}

PyDoc_STRVAR(pygpu_offscreen_color_object_doc, "Color object.\n\n:type: GLuint");
static PyObject *pygpu_offscreen_color_object_get(PyGPUOffScreen *self, void *UNUSED(type))
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self, "color object");
	return PyLong_FromLong(GPU_offscreen_color_object(self->ofs));
}

PyDoc_STRVAR(pygpu_offscreen_bind_doc,
"bind(save=True)\n"
"\n"
"   Bind the offscreen object.\n"
"\n"
"   :param save: save OpenGL current states\n"
"   :type save: bool"
);
static PyObject *pygpu_offscreen_bind(PyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
	bool save = true;
	static const char *kwlist[] = {"save", NULL};

	BPY_GPU_OFFSCREEN_CHECK_OBJ(self, "bind");

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "|O&:bind", (char **)(kwlist),
	        PyC_ParseBool, &save))
	{
		return NULL;
	}

	GPU_offscreen_bind(self->ofs, save);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_offscreen_unbind_doc,
"unbind(restore=True)\n"
"\n"
"   Unbind the offscreen object.\n"
"\n"
"   :param restore: restore OpenGL previous states\n"
"   :type restore: bool"
);
static PyObject *pygpu_offscreen_unbind(PyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
	bool restore = true;
	static const char *kwlist[] = {"restore", NULL};

	BPY_GPU_OFFSCREEN_CHECK_OBJ(self, "unbind");

	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds, "|O&:unbind", (char **)(kwlist),
	        PyC_ParseBool, &restore))
	{
		return NULL;
	}

	GPU_offscreen_unbind(self->ofs, restore);
	Py_RETURN_NONE;
}

/**
 * Use with PyArg_ParseTuple's "O&" formatting.
 */
static int pygpu_offscreen_check_matrix(PyObject *o, void *p)
{
	MatrixObject **PyMat_p = p;
	MatrixObject *PyMat = (MatrixObject *)o;

	if (!MatrixObject_Check(PyMat)) {
		PyErr_Format(PyExc_TypeError,
		             "matrix is not in a valid format (e.g., mathutils.Matrix)",
		             Py_TYPE(o)->tp_name);
		return 0;
	}

	if (BaseMath_ReadCallback(PyMat) == -1) {
		return 0;
	}

	if ((PyMat->num_col != 4) ||
	    (PyMat->num_row != 4))
	{
		PyErr_Format(PyExc_TypeError,
		             "matrix must have 4 rows and 4 columns",
		             Py_TYPE(o)->tp_name);
		return 0;
	}

	*PyMat_p = PyMat;
	return 1;
}

PyDoc_STRVAR(pygpu_offscreen_draw_view3d_doc,
"draw_view3d(modelview_matrix, projection_matrix)\n"
"\n"
"   Draw the 3d viewport in the offscreen object.\n"
"\n"
"   :param modelview_matrix: ModelView Matrix\n"
"   :type modelview_matrix: :class:`mathutils.Matrix`\n"
"   :param projection_matrix: Projection Matrix\n"
"   :type projection_matrix: :class:`mathutils.Matrix`"
);
static PyObject *pygpu_offscreen_draw_view3d(PyGPUOffScreen *self, PyObject *args, PyObject *kwds)
{
	MatrixObject *PyModelViewMatrix;
	MatrixObject *PyProjectionMatrix;
	bContext *C;
	View3D *v3d;
	ARegion *ar;

	static const char *kwlist[] = {"projection_matrix", "modelview_matrix", NULL};

	BPY_GPU_OFFSCREEN_CHECK_OBJ(self, "draw_view3d");

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&O&:draw_view3d", (char **)(kwlist),
	                                 pygpu_offscreen_check_matrix, &PyProjectionMatrix,
	                                 pygpu_offscreen_check_matrix, &PyModelViewMatrix))
	{
		return NULL;
	}

	C = BPy_GetContext();
	v3d = CTX_wm_view3d(C);
	ar = CTX_wm_region(C);

	if ((v3d == NULL) || (ar == NULL)) {
		PyErr_SetString(PyExc_SystemError, "draw_view3d: No valid view3d in the context");
		return NULL;
	}

	GPU_offscreen_draw_view3d(self->ofs, C, (float(*)[4])PyProjectionMatrix->matrix, (float(*)[4])PyModelViewMatrix->matrix);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(pygpu_offscreen_free_doc,
"free()\n"
"\n"
"   Free the offscreen object\n"
"   The framebuffer, texture and render objects will no longer be accessible."
);
static PyObject *pygpu_offscreen_free(PyGPUOffScreen *self)
{
	BPY_GPU_OFFSCREEN_CHECK_OBJ(self, "free");

	GPU_offscreen_free(self->ofs);
	self->ofs = NULL;
	Py_RETURN_NONE;
}

static void PyGPUOffScreen__tp_dealloc(PyGPUOffScreen *self)
{
	if (self->ofs)
		GPU_offscreen_free(self->ofs);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyGetSetDef bpy_gpu_offscreen_getseters[] = {
	{(char *)"color_object", (getter)pygpu_offscreen_color_object_get, (setter)NULL, pygpu_offscreen_color_object_doc, NULL},
	{(char *)"framebuffer_object", (getter)pygpu_offscreen_framebuffer_object_get, (setter)NULL, pygpu_offscreen_framebuffer_object_doc, NULL},
	{(char *)"width", (getter)pygpu_offscreen_width_get, (setter)NULL, pygpu_offscreen_width_doc, NULL},
	{(char *)"height", (getter)pygpu_offscreen_height_get, (setter)NULL, pygpu_offscreen_height_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

static struct PyMethodDef bpy_gpu_offscreen_methods[] = {
	{"bind", (PyCFunction)pygpu_offscreen_bind, METH_VARARGS | METH_KEYWORDS,pygpu_offscreen_bind_doc},
	{"unbind", (PyCFunction)pygpu_offscreen_unbind, METH_VARARGS | METH_KEYWORDS, pygpu_offscreen_unbind_doc},
	{"draw_view3d", (PyCFunction)pygpu_offscreen_draw_view3d, METH_VARARGS | METH_KEYWORDS, pygpu_offscreen_draw_view3d_doc},
	{"free", (PyCFunction)pygpu_offscreen_free, METH_NOARGS, pygpu_offscreen_free_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(py_gpu_offscreen_doc,
"GPUOffscreen(width, height, samples) -> new GPU Offscreen object"
"initialized to hold a framebuffer object of ``width`` x ``height`` with ``samples``.\n"
""
);
static PyTypeObject PyGPUOffScreen_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"GPUOffScreen",                              /* tp_name */
	sizeof(PyGPUOffScreen),                      /* tp_basicsize */
	0,                                           /* tp_itemsize */
	/* methods */
	(destructor)PyGPUOffScreen__tp_dealloc,      /* tp_dealloc */
	NULL,                                        /* tp_print */
	NULL,                                        /* tp_getattr */
	NULL,                                        /* tp_setattr */
	NULL,                                        /* tp_compare */
	NULL,                                        /* tp_repr */
	NULL,                                        /* tp_as_number */
	NULL,                                        /* tp_as_sequence */
	NULL,                                        /* tp_as_mapping */
	NULL,                                        /* tp_hash */
	NULL,                                        /* tp_call */
	NULL,                                        /* tp_str */
	NULL,                                        /* tp_getattro */
	NULL,                                        /* tp_setattro */
	NULL,                                        /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,                          /* tp_flags */
	py_gpu_offscreen_doc,                        /* Documentation string */
	NULL,                                        /* tp_traverse */
	NULL,                                        /* tp_clear */
	NULL,                                        /* tp_richcompare */
	0,                                           /* tp_weaklistoffset */
	NULL,                                        /* tp_iter */
	NULL,                                        /* tp_iternext */
	bpy_gpu_offscreen_methods,                   /* tp_methods */
	NULL,                                        /* tp_members */
	bpy_gpu_offscreen_getseters,                 /* tp_getset */
	NULL,                                        /* tp_base */
	NULL,                                        /* tp_dict */
	NULL,                                        /* tp_descr_get */
	NULL,                                        /* tp_descr_set */
	0,                                           /* tp_dictoffset */
	0,                                           /* tp_init */
	NULL,                                        /* tp_alloc */
	NULL,                                        /* tp_new */
	(freefunc)0,                                 /* tp_free */
	NULL,                                        /* tp_is_gc */
	NULL,                                        /* tp_bases */
	NULL,                                        /* tp_mro */
	NULL,                                        /* tp_cache */
	NULL,                                        /* tp_subclasses */
	NULL,                                        /* tp_weaklist */
	(destructor) NULL                            /* tp_del */
};

/* -------------------------------------------------------------------- */
/* GPU offscreen methods */

static PyObject *BPy_GPU_OffScreen_CreatePyObject(GPUOffScreen *ofs)
{
	PyGPUOffScreen *self;
	self = PyObject_New(PyGPUOffScreen, &PyGPUOffScreen_Type);
	self->ofs = ofs;
	return (PyObject *)self;
}

PyDoc_STRVAR(pygpu_offscreen_new_doc,
"new(width, height, samples)\n"
"\n"
"   Return a GPUOffScreen.\n"
"\n"
"   :param width: Horizontal dimension of the buffer\n"
"   :type width: int`\n"
"   :param width: Vertical dimension of the buffer\n"
"   :type width: int`\n"
"   :param samples: OpenGL samples\n"
"   :type samples: int\n"
"   :return: struct with GPUFrameBuffer, GPUTexture, GPUTexture.\n"
"   :rtype: :class:`gpu.OffScreenObject`"
);
static PyObject *pygpu_offscreen_new(PyObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
	GPUOffScreen *ofs;
	int width, height, samples;
	char err_out[256];

	static const char *kwlist[] = {"width", "height", "samples", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iii:new", (char **)(kwlist), &width, &height, &samples))
		return NULL;

	if (UINT_IS_NEG(width)) {
		PyErr_SetString(PyExc_ValueError, "negative 'width' given");
		return NULL;
	}

	if (UINT_IS_NEG(height)) {
		PyErr_SetString(PyExc_ValueError, "negative 'height' given");
		return NULL;
	}

	if (UINT_IS_NEG(samples)) {
		PyErr_SetString(PyExc_ValueError, "negative 'samples' given");
		return NULL;
	}

	ofs = GPU_offscreen_create(width, height, samples, err_out);

	if (ofs == NULL) {
		PyErr_Format(PyExc_Exception,
		             "gpu.offscreen.new(...) failed with '%s'",
		             err_out[0] ? err_out : "unknown error");
		return NULL;
	}

	return BPy_GPU_OffScreen_CreatePyObject(ofs);
}

static struct PyMethodDef BPy_GPU_offscreen_methods[] = {
	{"new", (PyCFunction)pygpu_offscreen_new, METH_VARARGS | METH_KEYWORDS, pygpu_offscreen_new_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(BPy_GPU_offscreen_doc,
"This module provides access to offscreen rendering functions."
);
static PyModuleDef BPy_GPU_offscreen_module_def = {
	PyModuleDef_HEAD_INIT,
	"gpu.offscreen",                             /* m_name */
	BPy_GPU_offscreen_doc,                       /* m_doc */
	0,                                           /* m_size */
	BPy_GPU_offscreen_methods,                   /* m_methods */
	NULL,                                        /* m_reload */
	NULL,                                        /* m_traverse */
	NULL,                                        /* m_clear */
	NULL,                                        /* m_free */
};

PyObject *BPyInit_gpu_offscreen(void)
{
	PyObject *submodule;

	/* Register the 'GPUOffscreen' class */
	if (PyType_Ready(&PyGPUOffScreen_Type)) {
		return NULL;
	}

	submodule = PyModule_Create(&BPy_GPU_offscreen_module_def);

	return submodule;
}

#undef UINT_IS_NEG
#undef BPY_GPU_OFFSCREEN_CHECK_OBJ
