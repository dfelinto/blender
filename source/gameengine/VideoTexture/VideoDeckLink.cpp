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

/** \file gameengine/VideoTexture/VideoDeckLink.cpp
 *  \ingroup bgevideotex
 */

#ifdef WITH_DECKLINK

// INT64_C fix for some linux machines (C99ism)
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <stdint.h>

#include "VideoDeckLink.h"
#include "GL/glew.h"
#include "dvpapi_gl.h"

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include <string>

#include "Exception.h"


// macro for exception handling and logging
#define CATCH_EXCP catch (Exception & exp) \
{ exp.report(); m_status = SourceError; }

// class RenderVideo

// constructor
VideoDeckLink::VideoDeckLink (HRESULT * hRslt) : VideoBase()
{
}

// destructor
VideoDeckLink::~VideoDeckLink ()
{
}

void VideoDeckLink::refresh(void)
{
    m_avail = false;
}

// release components
bool VideoDeckLink::release()
{
	// release
	return true;
}


// open video file
void VideoDeckLink::openFile (char *filename)
{
	// open base class
	VideoBase::openFile(filename);

}


// open video capture device
void VideoDeckLink::openCam (char *file, short camIdx)
{
	// open base class
	VideoBase::openCam(file, camIdx);
}

// play video
bool VideoDeckLink::play (void)
{
	try
	{
		// if object is able to play
		if (VideoBase::play())
		{
			// return success
			return true;
		}
	}
	CATCH_EXCP;
	return false;
}


// pause video
bool VideoDeckLink::pause (void)
{
	try
	{
		if (VideoBase::pause())
		{
			return true;
		}
	}
	CATCH_EXCP;
	return false;
}

// stop video
bool VideoDeckLink::stop (void)
{
	try
	{
		VideoBase::stop();
		return true;
	}
	CATCH_EXCP;
	return false;
}


// set video range
void VideoDeckLink::setRange (double start, double stop)
{
}

// set framerate
void VideoDeckLink::setFrameRate (float rate)
{
	VideoBase::setFrameRate(rate);
}


// image calculation
// load frame from video
void VideoDeckLink::calcImage (unsigned int texId, double ts)
{
}


// python methods

// object initialization
static int VideoDeckLink_init(PyObject *pySelf, PyObject *args, PyObject *kwds)
{
	PyImage *self = reinterpret_cast<PyImage*>(pySelf);
	// parameters - video source
	// file name or format type for capture (only for Linux: video4linux or dv1394)
	char * file = NULL;
	// capture device number
	short capt = -1;
	// capture width, only if capt is >= 0
	short width = 0;
	// capture height, only if capt is >= 0
	short height = 0;
	// capture rate, only if capt is >= 0
	float rate = 25.f;

	static const char *kwlist[] = {"file", "capture", "rate", "width", "height", NULL};

	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|hfhh",
		const_cast<char**>(kwlist), &file, &capt, &rate, &width, &height))
		return -1; 

	try
	{
		// create video object
		Video_init<VideoDeckLink>(self);

		// open video source
		Video_open(getVideo(self), file, capt);
	}
	catch (Exception & exp)
	{
		exp.report();
		return -1;
	}
	// initialization succeded
	return 0;
}

// methods structure
static PyMethodDef videoMethods[] =
{ // methods from VideoBase class
	{"play", (PyCFunction)Video_play, METH_NOARGS, "Play (restart) video"},
	{"pause", (PyCFunction)Video_pause, METH_NOARGS, "pause video"},
	{"stop", (PyCFunction)Video_stop, METH_NOARGS, "stop video (play will replay it from start)"},
	{"refresh", (PyCFunction)Video_refresh, METH_NOARGS, "Refresh video - get its status"},
	{NULL}
};
// attributes structure
static PyGetSetDef videoGetSets[] =
{ // methods from VideoBase class
	{(char*)"status", (getter)Video_getStatus, NULL, (char*)"video status", NULL},
	{(char*)"range", (getter)Video_getRange, (setter)Video_setRange, (char*)"replay range", NULL},
	{(char*)"repeat", (getter)Video_getRepeat, (setter)Video_setRepeat, (char*)"repeat count, -1 for infinite repeat", NULL},
	{(char*)"framerate", (getter)Video_getFrameRate, (setter)Video_setFrameRate, (char*)"frame rate", NULL},
	// attributes from ImageBase class
	{(char*)"valid", (getter)Image_valid, NULL, (char*)"bool to tell if an image is available", NULL},
	{(char*)"image", (getter)Image_getImage, NULL, (char*)"image data", NULL},
	{(char*)"size", (getter)Image_getSize, NULL, (char*)"image size", NULL},
	{(char*)"scale", (getter)Image_getScale, (setter)Image_setScale, (char*)"fast scale of image (near neighbor)", NULL},
	{(char*)"flip", (getter)Image_getFlip, (setter)Image_setFlip, (char*)"flip image vertically", NULL},
	{(char*)"filter", (getter)Image_getFilter, (setter)Image_setFilter, (char*)"pixel filter", NULL},
	{NULL}
};

// python type declaration
PyTypeObject VideoDeckLinkType =
{ 
	PyVarObject_HEAD_INIT(NULL, 0)
	"VideoTexture.VideoDeckLink",   /*tp_name*/
	sizeof(PyImage),          /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Image_dealloc, /*tp_dealloc*/
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
	"DeckLink video source",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	videoMethods,    /* tp_methods */
	0,                   /* tp_members */
	videoGetSets,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)VideoDeckLink_init,     /* tp_init */
	0,                         /* tp_alloc */
	Image_allocNew,           /* tp_new */
};


#endif		// WITH_DECKLINK

