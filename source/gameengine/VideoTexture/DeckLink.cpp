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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2007 The Zdeno Ash Miklas
 *
 * This source file is part of VideoTexture library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/VideoTexture/Texture.cpp
 *  \ingroup bgevideotex
 */

#ifdef WITH_DECKLINK

// implementation

#include "PyObjectPlus.h"
#include <structmember.h>

#include "KX_GameObject.h"
#include "KX_Light.h"
#include "RAS_MeshObject.h"
#include "RAS_ILightObject.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_image_types.h"
#include "IMB_imbuf_types.h"
#include "BKE_image.h"

#include "MEM_guardedalloc.h"

#include "KX_KetsjiEngine.h"
#include "KX_PythonInit.h"
#include "DeckLink.h"

#include <memory.h>

// macro for exception handling and logging
#define CATCH_EXCP catch (Exception & exp) \
{ exp.report(); return NULL; }


// DeckLink object allocation
static PyObject *DeckLink_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	// allocate object
	DeckLink * self = reinterpret_cast<DeckLink*>(type->tp_alloc(type, 0));
	// initialize object structure
	self->m_source = NULL;
	self->m_lastClock = 0.0;
	// return allocated object
	return reinterpret_cast<PyObject*>(self);
}


// forward declaration
PyObject *DeckLink_close(DeckLink *self);
int DeckLink_setSource(DeckLink *self, PyObject *value, void *closure);


// DeckLink object deallocation
static void DeckLink_dealloc(DeckLink *self)
{
	// release renderer
	Py_XDECREF(self->m_source);
	// close decklink
	PyObject *ret = DeckLink_close(self);
	Py_DECREF(ret);
	// release object
	Py_TYPE((PyObject *)self)->tp_free((PyObject *)self);
}


ExceptionID AutoDetectionNotAvail;
ExpDesc AutoDetectionNotAvailDesc(AutoDetectionNotAvail, "Auto detection not yet available");

// DeckLink object initialization
static int DeckLink_init(DeckLink *self, PyObject *args, PyObject *kwds)
{
	// parameters - game object with video texture
	PyObject *obj = NULL;
	// material ID
	short cardIdx = 0;
	// texture ID
	char *format = NULL;

	static const char *kwlist[] = {"cardIdx", "format", NULL};
	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|hs",
		const_cast<char**>(kwlist), &cardIdx, &format))
		return -1; 

	
	if (format == NULL)
	{
		THRWEXCP(AutoDetectionNotAvail, S_OK);
	}
	// initialization succeeded
	return 0;
}


// close added decklink
PyObject *DeckLink_close(DeckLink * self)
{
	Py_RETURN_NONE;
}


// refresh decklink key frame
static PyObject *DeckLink_refresh(DeckLink *self, PyObject *args)
{
	// get parameter - refresh source
	PyObject *param;
	double ts = -1.0;

	if (!PyArg_ParseTuple(args, "O|d:refresh", &param, &ts) || !PyBool_Check(param))
	{
		// report error
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return NULL;
	}
	// some trick here: we are in the business of loading a key frame in decklink,
	// no use to do it if we are still in the same rendering frame.
	// We find this out by looking at the engine current clock time
	KX_KetsjiEngine* engine = KX_GetActiveEngine();
	if (engine->GetClockTime() != self->m_lastClock) 
	{
		self->m_lastClock = engine->GetClockTime();
		// set source refresh
		bool refreshSource = (param == Py_True);
		// try to process key frame from source
		try
		{
			// if source is available
			if (self->m_source != NULL)
			{
				// get texture
				unsigned int * texture = self->m_source->m_image->getImage(0, ts);
				// if texture is available
				if (texture != NULL)
				{
					;
				}
				// refresh texture source, if required
				if (refreshSource) self->m_source->m_image->refresh();
			}
		}
		CATCH_EXCP;
	}
	Py_RETURN_NONE;
}

// get source object
static PyObject *DeckLink_getSource(DeckLink *self, PyObject *value, void *closure)
{
	// if source exists
	if (self->m_source != NULL)
	{
		Py_INCREF(self->m_source);
		return reinterpret_cast<PyObject*>(self->m_source);
	}
	// otherwise return None
	Py_RETURN_NONE;
}


// set source object
int DeckLink_setSource(DeckLink *self, PyObject *value, void *closure)
{
	// check new value
	if (value == NULL || !pyImageTypes.in(Py_TYPE(value)))
	{
		// report value error
		PyErr_SetString(PyExc_TypeError, "Invalid type of value");
		return -1;
	}
	// increase ref count for new value
	Py_INCREF(value);
	// release previous
	Py_XDECREF(self->m_source);
	// set new value
	self->m_source = reinterpret_cast<PyImage*>(value);
	// return success
	return 0;
}


// class DeckLink methods
static PyMethodDef decklinkMethods[] =
{
	{ "close", (PyCFunction)DeckLink_close, METH_NOARGS, "Close dynamic decklink and restore original"},
	{ "refresh", (PyCFunction)DeckLink_refresh, METH_VARARGS, "Refresh decklink from source"},
	{NULL}  /* Sentinel */
};

// class DeckLink attributes
static PyGetSetDef decklinkGetSets[] =
{ 
	{(char*)"source", (getter)DeckLink_getSource, (setter)DeckLink_setSource, (char*)"source of decklink", NULL},
	{NULL}
};


// class DeckLink declaration
PyTypeObject DeckLinkType =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"VideoTexture.DeckLink",   /*tp_name*/
	sizeof(DeckLink),           /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)DeckLink_dealloc,/*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	&imageBufferProcs,         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /*tp_flags*/
	"DeckLink objects",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	decklinkMethods,      /* tp_methods */
	0,                   /* tp_members */
	decklinkGetSets,            /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)DeckLink_init,    /* tp_init */
	0,                         /* tp_alloc */
	DeckLink_new,               /* tp_new */
};

#endif	/* WITH_DECKLINK */
