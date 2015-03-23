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
#include <string.h>

#include "VideoDeckLink.h"
#include "dvpapi_gl.h"

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "Exception.h"

static struct 
{
	const char *name;
	BMDDisplayMode mode;
} sModeStringTab[] = {
	{ "NTSC", bmdModeNTSC },
	{ "NTSC2398", bmdModeNTSC2398 },
	{ "PAL", bmdModePAL },
	{ "NTSCp", bmdModeNTSCp },
	{ "PALp", bmdModePALp },

	/* HD 1080 Modes */

	{ "HD1080p2398", bmdModeHD1080p2398 },
	{ "HD1080p24", bmdModeHD1080p24 },
	{ "HD1080p25", bmdModeHD1080p25 },
	{ "HD1080p2997", bmdModeHD1080p2997 },
	{ "HD1080p30", bmdModeHD1080p30 },
	{ "HD1080i50", bmdModeHD1080i50 },
	{ "HD1080i5994", bmdModeHD1080i5994 },
	{ "HD1080i6000", bmdModeHD1080i6000 },
	{ "HD1080p50", bmdModeHD1080p50 },
	{ "HD1080p5994", bmdModeHD1080p5994 },
	{ "HD1080p6000", bmdModeHD1080p6000 },

	/* HD 720 Modes */

	{ "HD720p50", bmdModeHD720p50 },
	{ "HD720p5994", bmdModeHD720p5994 },
	{ "HD720p60", bmdModeHD720p60 },

	/* 2k Modes */

	{ "2k2398", bmdMode2k2398 },
	{ "2k24", bmdMode2k24 },
	{ "2k25", bmdMode2k25 },

	/* DCI Modes (output only) */

	{ "2kDCI2398", bmdMode2kDCI2398 },
	{ "2kDCI24", bmdMode2kDCI24 },
	{ "2kDCI25", bmdMode2kDCI25 },

	/* 4k Modes */

	{ "4K2160p2398", bmdMode4K2160p2398 },
	{ "4K2160p24", bmdMode4K2160p24 },
	{ "4K2160p25", bmdMode4K2160p25 },
	{ "4K2160p2997", bmdMode4K2160p2997 },
	{ "4K2160p30", bmdMode4K2160p30 },
	{ "4K2160p50", bmdMode4K2160p50 },
	{ "4K2160p5994", bmdMode4K2160p5994 },
	{ "4K2160p60", bmdMode4K2160p60 },
	// sentinel
	{ NULL }
};


static struct 
{
	const char *name;
	BMDPixelFormat format;
} sFormatStringTab[] = {
	{ "8BitYUV", bmdFormat8BitYUV },
	{ "10BitYUV", bmdFormat10BitYUV },
	{ "8BitARGB", bmdFormat8BitARGB },
	{ "8BitBGRA", bmdFormat8BitBGRA },
	{ "10BitRGB", bmdFormat10BitRGB },
	{ "12BitRGB", bmdFormat12BitRGB },
	{ "12BitRGBLE", bmdFormat12BitRGBLE },
	{ "10BitRGBXLE", bmdFormat10BitRGBXLE },
	{ "10BitRGBX", bmdFormat10BitRGBX },
	// sentinel
	{ NULL }
};
	
// macro for exception handling and logging
#define CATCH_EXCP catch (Exception & exp) \
{ exp.report(); m_status = SourceError; }

// class RenderVideo

// constructor
VideoDeckLink::VideoDeckLink (HRESULT * hRslt) : VideoBase(),
mDLInput(NULL),
mUse3D(false),
mHasDvp(false),
mHasAMDPinnedMemory(false),
mFastTransferAvailable(false),
mFrameWidth(0),
mFrameHeight(0),
mTextureWidth(0),
mTextureHeight(0),
mUnpinnedTextureBuffer(0),
mCaptureTexture(0),
mpAllocator(NULL),
mpCaptureDelegate(NULL),
mpCacheFrame(NULL)
{
	mDisplayMode = (BMDDisplayMode)0;
	mPixelFormat = (BMDPixelFormat)0;
	pthread_mutex_init(&mCacheMutex, NULL);
}

// destructor
VideoDeckLink::~VideoDeckLink ()
{
	if (mDLInput != NULL)
	{
		// Cleanup for Capture
		mDLInput->SetCallback(NULL);
		mDLInput->Release();
		mDLInput = NULL;
	}
	if (mpAllocator) 
	{
		delete mpAllocator;
		mpAllocator = NULL;
	}
	if (mpCaptureDelegate)
	{
		delete mpCaptureDelegate;
		mpCaptureDelegate = NULL;
	}
	if (mpCacheFrame)
		mpCacheFrame->Release();
	if (mUnpinnedTextureBuffer)
		glDeleteBuffers(1, &mUnpinnedTextureBuffer);
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

ExceptionID SourceVideoOnlyCapture, VideoDeckLinkBadFormat, VideoDeckLinkOpenCard, VideoDeckLinkInternalError;
ExpDesc SourceVideoOnlyCaptureDesc(SourceVideoOnlyCapture, "This video source only allows live capture");
ExpDesc VideoDeckLinkBadFormatDesc(VideoDeckLinkBadFormat, "Invalid or unsupported capture format, should be <mode>/<pixel>[/3D]");
ExpDesc VideoDeckLinkOpenCardDesc(VideoDeckLinkOpenCard, "Cannot open capture card, check if driver installed");
ExpDesc VideoDeckLinkInternalErrorDEsc(VideoDeckLinkInternalError, "DeckLink API internal error, please report");

// open video file
void VideoDeckLink::openFile (char *filename)
{
	// only live capture on this device
	THRWEXCP(SourceVideoOnlyCapture, S_OK);
}


// open video capture device
void VideoDeckLink::openCam (char *format, short camIdx)
{
	IDeckLinkDisplayModeIterator*	pDLDisplayModeIterator;
	BMDDisplayModeSupport			modeSupport;
	IDeckLinkDisplayMode*			pDLDisplayMode;
	IDeckLinkIterator*				pIterator;
	BMDTimeValue					frameDuration;
	BMDTimeScale					frameTimescale;
	IDeckLink*						pDL;
	u_int displayFlags, inputFlags; 
	char *pPixel, *p3D;
	size_t len;
	int i;

	// open the card
	pIterator = BMD_CreateDeckLinkIterator();
	pDL = NULL;
	if (pIterator) 
	{
		i = 0;
		while (pIterator->Next(&pDL) == S_OK) 
		{
			if (i == camIdx) 
			{
				if (pDL->QueryInterface(IID_IDeckLinkInput, (void**)&mDLInput) != S_OK)
					mDLInput = NULL;
				break;
			}
			i++;
		}
	}
	if (!mDLInput)
		THRWEXCP(VideoDeckLinkOpenCard, S_OK);

	// format is constructed as <displayMode>/<pixelFormat>[/3D]
	// <displayMode> takes the form of BMDDisplayMode identifier minus the 'bmdMode' prefix.
	//               This implementation understands all the modes defined in SDK 10.3.1 but you can alternatively
	//               use the 4 characters internal representation of the mode (e.g. 'HD1080p24' == '24ps')
	// <pixelFormat> takes the form of BMDPixelFormat identifier minus the 'bmdFormat' prefix.
	//               This implementation understand all the formats defined in SDK 10.32.1 but you can alternatively
	//               use the 4 characters internal representation of the format (e.g. '10BitRGB' == 'r210')
	// Not all combinations of mode and pixel format are possible and it also depends on the card!
	// Use /3D postfix if you are capturing a 3D stream with frame packing
	// Example: To capture FullHD 1920x1080@24Hz with 3D packing and 4:4:4 10 bits RGB pixel format, use
	// "HD1080p24/10BitRGB/3D"  (same as "24ps/r210/3D")
	// (this will be the normal capture format for FullHD on the DeckLink 4k extreme)

	if ((pPixel = strchr(format, '/')) == NULL ||
		((p3D = strchr(pPixel + 1, '/')) != NULL && strcmp(p3D, "/3D")))
		THRWEXCP(VideoDeckLinkBadFormat, S_OK);
	mUse3D = (p3D) ? true : false;
	// read the mode
	len = (size_t)(pPixel - format);
	pPixel++;
	for (i = 0; sModeStringTab[i].name != NULL; i++) {
		if (strlen(sModeStringTab[i].name) == len &&
			!strncmp(sModeStringTab[i].name, format, len)) 
		{
			mDisplayMode = sModeStringTab[i].mode;
			break;
		}
	}
	if (!mDisplayMode) 
	{
		if (len != 4)
			THRWEXCP(VideoDeckLinkBadFormat, S_OK);
		// assume the user entered directly the mode value as a 4 char string
		mDisplayMode = (BMDDisplayMode)((((u_int)format[0]) << 24) + (((u_int)format[1]) << 16) + (((u_int)format[2]) << 8) + ((u_int)format[3]));
	}
	// read the pixel
	len = ((mUse3D) ? (size_t)(p3D - pPixel) : strlen(pPixel));
	for (i = 0; sFormatStringTab[i].name != NULL; i++) 
	{
		if (strlen(sFormatStringTab[i].name) == len &&
			!strncmp(sFormatStringTab[i].name, format, len)) 
		{
			mPixelFormat = sFormatStringTab[i].format;
			break;
		}
	}
	if (!mPixelFormat) 
	{
		if (len != 4)
			THRWEXCP(VideoDeckLinkBadFormat, S_OK);
		// assume the user entered directly the mode value as a 4 char string
		mPixelFormat = (BMDPixelFormat)((((u_int)pPixel[0]) << 24) + (((u_int)pPixel[1]) << 16) + (((u_int)pPixel[2]) << 8) + ((u_int)pPixel[3]));
	}
	
	
	// check if display mode and pixel format are supported
	if (mDLInput->GetDisplayModeIterator(&pDLDisplayModeIterator) != S_OK)
		THRWEXCP(VideoDeckLinkInternalError, S_OK);

	pDLDisplayMode = NULL;
	displayFlags = (mUse3D) ? bmdDisplayModeSupports3D : 0;
	inputFlags = (mUse3D) ? bmdVideoInputDualStream3D : bmdVideoInputFlagDefault;
	while (pDLDisplayModeIterator->Next(&pDLDisplayMode) == S_OK) 
	{
		if (   pDLDisplayMode->GetDisplayMode() == mDisplayMode 
			&& (pDLDisplayMode->GetFlags() & displayFlags) == displayFlags
			&& mDLInput->DoesSupportVideoMode(mDisplayMode, mPixelFormat, inputFlags, &modeSupport, NULL) == S_OK
			&& modeSupport == bmdDisplayModeSupported)
			break;
		pDLDisplayMode->Release();
		pDLDisplayMode = NULL;
	}
	pDLDisplayModeIterator->Release();

	if (pDLDisplayMode == NULL)
		THRWEXCP(VideoDeckLinkBadFormat, S_OK);

	mFrameWidth = pDLDisplayMode->GetWidth();
	mFrameHeight = pDLDisplayMode->GetHeight();
	mTextureHeight = (mUse3D) ? 2 * mFrameHeight : mFrameHeight;
	pDLDisplayMode->GetFrameRate(&frameDuration, &frameTimescale);
	// for information, in case the application wants to know
	m_size[0] = mFrameWidth;
	m_size[1] = mTextureHeight;
	m_frameRate = (float)frameTimescale / (float)frameDuration;

	switch (mPixelFormat)
	{
	case bmdFormat8BitYUV:
		// 2 pixels per word
		mTextureStride = mFrameWidth * 2;
		mTextureWidth = mFrameWidth / 2;
		mTextureFormat = GL_RED_INTEGER;
		mTextureType = GL_UNSIGNED_INT;
		break;
	case bmdFormat10BitYUV:
		// 6 pixels in 4 words, rounded to 48 pixels
		mTextureStride = ((mFrameWidth + 47) / 48) * 128;
		mTextureWidth = ((mFrameWidth + 5) / 6) * 4;
		mTextureFormat = GL_RED_INTEGER;
		mTextureType = GL_UNSIGNED_INT;
		break;
	case bmdFormat8BitARGB:
		mTextureStride = mFrameWidth * 4;
		mTextureWidth = mFrameWidth;
		mTextureFormat = GL_BGRA;
		mTextureType = GL_UNSIGNED_INT_8_8_8_8;
		break;
	case bmdFormat8BitBGRA:
		mTextureStride = mFrameWidth * 4;
		mTextureWidth = mFrameWidth;
		mTextureFormat = GL_BGRA;
		mTextureType = GL_UNSIGNED_INT_8_8_8_8_REV;
		break;
	case bmdFormat10BitRGBXLE:
	case bmdFormat10BitRGBX:
	case bmdFormat10BitRGB:
		mTextureStride = ((mFrameWidth + 63) / 64) * 256;
		mTextureWidth = mFrameWidth;
		mTextureFormat = GL_RED_INTEGER;
		mTextureType = GL_UNSIGNED_INT;
		break;
	case bmdFormat12BitRGB:
	case bmdFormat12BitRGBLE:
		mTextureStride = (mFrameWidth * 36) / 8;
		mTextureWidth = (mFrameWidth / 8) * 9;
		mTextureFormat = GL_RED_INTEGER;
		mTextureType = GL_UNSIGNED_INT;
		break;
	// for unknown pixel format, this will be resolved when a frame arrives
	}

#ifdef WIN32
	// In windows, AMD_pinned_memory option is not available, 
	// we must use special DVP API only available for Quadro cards
	const char* renderer = (const char *)glGetString(GL_RENDERER);
	mHasDvp = (strstr(renderer, "Quadro") != NULL);
	// TBD: only Quadro K4000 and above have support for GPUDirect, test that
#endif
	if (GLEW_AMD_pinned_memory)
		mHasAMDPinnedMemory = true;

	mFastTransferAvailable = mHasDvp || mHasAMDPinnedMemory;
	if (!mFastTransferAvailable)
	{
		glGenBuffers(1, &mUnpinnedTextureBuffer);
	}
	// custom allocator, 3 frame in cache should be enough
	mpAllocator = new PinnedMemoryAllocator(3);

	if (mDLInput->SetVideoInputFrameMemoryAllocator(mpAllocator) != S_OK)
		THRWEXCP(VideoDeckLinkInternalError, S_OK);

	if (mDLInput->EnableVideoInput(mDisplayMode, mPixelFormat, ((mUse3D) ? bmdVideoInputDualStream3D : bmdVideoInputFlagDefault)) != S_OK)
		// this shouldn't failed, we tested above
		THRWEXCP(VideoDeckLinkInternalError, S_OK); 

	mpCaptureDelegate = new CaptureDelegate(this);
	if (mDLInput->SetCallback(mpCaptureDelegate) != S_OK)
		THRWEXCP(VideoDeckLinkInternalError, S_OK);

	// open base class
	VideoBase::openCam(format, camIdx);

	// ready to capture, will start when application calls play()
}

// play video
bool VideoDeckLink::play (void)
{
	try
	{
		// if object is able to play
		if (VideoBase::play())
		{
			mDLInput->FlushStreams();
			return (mDLInput->StartStreams() == S_OK);
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
			mDLInput->PauseStreams();
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
		mDLInput->StopStreams();
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
}


// image calculation
// send cache frame directly to GPU
void VideoDeckLink::calcImage (unsigned int texId, double ts)
{
	IDeckLinkVideoInputFrame* pFrame;
	LockCache();
	pFrame = mpCacheFrame;
	mpCacheFrame = NULL;
	UnlockCache();
	if (pFrame)
	{
		// TBD
		pFrame->Release();
	}
}

// A frame is available from the board
// Called from an internal thread, just pass the frame to the main thread
void VideoDeckLink::VideoFrameArrived(IDeckLinkVideoInputFrame* inputFrame)
{
	IDeckLinkVideoInputFrame* pOldFrame;
	LockCache();
	pOldFrame = mpCacheFrame;
	mpCacheFrame = inputFrame;
	inputFrame->AddRef();
	UnlockCache();
	// old frame no longer needed, just release it
	if (pOldFrame)
		pOldFrame->Release();
}

// python methods

// object initialization
static int VideoDeckLink_init(PyObject *pySelf, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "format", "capture", NULL };
	PyImage *self = reinterpret_cast<PyImage*>(pySelf);
	// see openCam for a description of format
	char * format = NULL;
	// capture device number, i.e. DeckLink card number, default first one
	short capt = 0;

	if (!GLEW_VERSION_1_5)
	{
		PyErr_SetString(PyExc_RuntimeError, "VideoDeckLink requires at least OpenGL 1.5");
		return -1;
	}
	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|h",
		const_cast<char**>(kwlist), &format, &capt))
		return -1; 

	try
	{
		// create video object
		Video_init<VideoDeckLink>(self);

		// open video source, control comes back to VideoDeckLink::openCam
		Video_open(getVideo(self), format, capt);
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
	{(char*)"framerate", (getter)Video_getFrameRate, NULL, (char*)"frame rate", NULL},
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


////////////////////////////////////////////
// PinnedMemoryAllocator
////////////////////////////////////////////

// PinnedMemoryAllocator implements the IDeckLinkMemoryAllocator interface and can be used instead of the
// built-in frame allocator, by setting with SetVideoInputFrameMemoryAllocator() or SetVideoOutputFrameMemoryAllocator().
//
// For this sample application a custom frame memory allocator is used to ensure each address
// of frame memory is aligned on a 4kB boundary required by the OpenGL pinned memory extension.
// If the pinned memory extension is not available, this allocator will still be used and
// demonstrates how to cache frame allocations for efficiency.
//
// The frame cache delays the releasing of buffers until the cache fills up, thereby avoiding an
// allocate plus pin operation for every frame, followed by an unpin and deallocate on every frame.

PinnedMemoryAllocator::PinnedMemoryAllocator(unsigned cacheSize) :
mRefCount(1),
mBufferCacheSize(cacheSize)
{
	pthread_mutex_init(&mMutex, NULL);
}

PinnedMemoryAllocator::~PinnedMemoryAllocator()
{
}

bool PinnedMemoryAllocator::PinBuffer(void *address)
{
#ifdef WIN32
	bool rc = false;
	Lock();
	if (mPinnedBuffer.count(address) > 0)
	{
		rc = true;
	} 
	else if (mAllocatedSize.count(address) > 0)
	{
		if (VirtualLock(address, mAllocatedSize[address]))
		{
			mPinnedBuffer.insert(address);
			rc = true;
		}
	}
	Unlock();
	return rc;
#else
	// Linux doesn't have the equivalent?
	return true;
#endif
}

void PinnedMemoryAllocator::UnpinBuffer(void* address)
{
#ifdef WIN32
	if (mPinnedBuffer.count(address) > 0)
	{
		VirtualUnlock(address, mAllocatedSize[address]);
		mPinnedBuffer.erase(address);
	}
#endif
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::QueryInterface(REFIID /*iid*/, LPVOID* /*ppv*/)
{
	return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE		PinnedMemoryAllocator::AddRef(void)
{
	ULONG newCount;
	Lock();
	newCount = ++mRefCount;
	Unlock();
	return newCount;
}

ULONG STDMETHODCALLTYPE		PinnedMemoryAllocator::Release(void)
{
	int newCount;
	Lock();
	newCount = --mRefCount;
	Unlock();
	if (newCount == 0)
		delete this;
	return (ULONG)newCount;
}

// IDeckLinkMemoryAllocator methods
HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::AllocateBuffer(unsigned long bufferSize, void* *allocatedBuffer)
{
	Lock();
	if (mBufferCache.empty())
	{
		// Allocate memory on a page boundary
		// Note: aligned alloc exist in Blender but only for small alignment, use direct allocation then.
		// Note: the DeckLink API tries to allocate up to 65 buffer in advance, we will limit this to 3
		//       because we don't need any caching
		if (mAllocatedSize.size() >= 3)
			return E_OUTOFMEMORY;

#ifdef WIN32
		*allocatedBuffer = VirtualAlloc(NULL, bufferSize, MEM_COMMIT | MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE);
#else
		if (posix_memalign(allocatedBuffer, 4096, bufferSize) != 0)
			*allocatedBuffer = NULL;
#endif
		mAllocatedSize[*allocatedBuffer] = bufferSize;
	}
	else
	{
		// Re-use most recently ReleaseBuffer'd address
		*allocatedBuffer = mBufferCache.back();
		mBufferCache.pop_back();
	}
	Unlock();
	return (*allocatedBuffer) ? S_OK : E_OUTOFMEMORY;
}

HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::ReleaseBuffer(void* buffer)
{
	Lock();
	if (mBufferCache.size() < mBufferCacheSize)
	{
		mBufferCache.push_back(buffer);
	}
	else
	{
		// No room left in cache, so un-pin (if it was pinned) and free this buffer
		UnpinBuffer(buffer);
#ifdef WIN32
		VirtualFree(buffer, 0, MEM_RELEASE);
#else
		free(buffer);
#endif
		mAllocatedSize.erase(buffer);
	}
	Unlock();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::Commit()
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::Decommit()
{
	Lock();
	while (!mBufferCache.empty())
	{
		// Cleanup any frames allocated and pinned in AllocateBuffer() but not freed in ReleaseBuffer()
		UnpinBuffer(mBufferCache.back());
#ifdef WIN32
		VirtualFree(mBufferCache.back(), 0, MEM_RELEASE);
#else
		free(mBufferCache.back());
#endif
		mBufferCache.pop_back();
	}
	Unlock();
	return S_OK;
}

////////////////////////////////////////////
// DeckLink Capture Delegate Class
////////////////////////////////////////////
CaptureDelegate::CaptureDelegate(VideoDeckLink* pOwner) : mpOwner(pOwner)
{
}

HRESULT	CaptureDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame* inputFrame, IDeckLinkAudioInputPacket* /*audioPacket*/)
{
	if (!inputFrame)
	{
		// It's possible to receive a NULL inputFrame, but a valid audioPacket. Ignore audio-only frame.
		return S_OK;
	}
	if ((inputFrame->GetFlags() & bmdFrameHasNoInputSource) == bmdFrameHasNoInputSource)
	{
		// let's not bother transferring frames if there is no source
		return S_OK;
	}
	mpOwner->VideoFrameArrived(inputFrame);
	return S_OK;
}

HRESULT	CaptureDelegate::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{
	return S_OK;
}

#endif		// WITH_DECKLINK

