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

/** \file VideoDeckLink.h
 *  \ingroup bgevideotex
 */

#ifndef __VIDEODECKLINK_H__
#define __VIDEODECKLINK_H__

#ifdef WITH_DECKLINK

/* this needs to be parsed with __cplusplus defined before included through DeckLink_compat.h */
#if defined(__FreeBSD__)
#  include <inttypes.h>
#endif
#include <map>
#include <set>
extern "C" {
#include <pthread.h>
#include "DNA_listBase.h"
#include "BLI_threads.h"
#include "BLI_blenlib.h"
}
#include "GL/glew.h"
#include "DeckLinkAPI.h"
#include "VideoBase.h"

class PinnedMemoryAllocator;

// type VideoDeckLink declaration
class VideoDeckLink : public VideoBase
{
	friend class CaptureDelegate;
public:
	/// constructor
	VideoDeckLink (HRESULT * hRslt);
	/// destructor
	virtual ~VideoDeckLink ();

	/// set initial parameters
	void initParams (short width, short height, float rate, bool image=false);
	/// open video/image file
	virtual void openFile(char *file);
	/// open video capture device
	virtual void openCam(char *driver, short camIdx);

	/// release video source
	virtual bool release (void);
    /// overwrite base refresh to handle fixed image
    virtual void refresh(void);
	/// play video
	virtual bool play (void);
	/// pause video
	virtual bool pause (void);
	/// stop video
	virtual bool stop (void);
	/// set play range
	virtual void setRange (double start, double stop);
	/// set frame rate
	virtual void setFrameRate (float rate);

protected:
	// format and codec information
	/// image calculation
	virtual void calcImage (unsigned int texId, double ts);

private:
	void					VideoFrameArrived(IDeckLinkVideoInputFrame* inputFrame);
	void					LockCache()
	{
		pthread_mutex_lock(&mCacheMutex);
	}
	void					UnlockCache()
	{
		pthread_mutex_unlock(&mCacheMutex);
	}

	IDeckLinkInput*			mDLInput;
	BMDDisplayMode			mDisplayMode;
	BMDPixelFormat			mPixelFormat;
	bool					mUse3D;
	bool					mHasDvp;
	bool					mHasAMDPinnedMemory;
	bool					mFastTransferAvailable;
	u_int					mFrameWidth;
	u_int					mFrameHeight;
	u_int					mTextureWidth;
	u_int					mTextureHeight;
	u_int					mTextureStride;
	GLenum					mTextureFormat;
	GLenum					mTextureType;
	GLuint					mUnpinnedTextureBuffer;
	GLuint					mCaptureTexture;
	PinnedMemoryAllocator*	mpAllocator;
	CaptureDelegate*		mpCaptureDelegate;

	// cache frame in transit between the callback thread and the main BGE thread
	// keep only one frame in cache because we just want to keep up with real time
	pthread_mutex_t			mCacheMutex;
	IDeckLinkVideoInputFrame* mpCacheFrame;

};

inline VideoDeckLink *getDeckLink(PyImage *self)
{
	return static_cast<VideoDeckLink*>(self->m_image);
}

////////////////////////////////////////////
// PinnedMemoryAllocator
////////////////////////////////////////////
class PinnedMemoryAllocator : public IDeckLinkMemoryAllocator
{
public:
	PinnedMemoryAllocator(unsigned cacheSize);
	virtual ~PinnedMemoryAllocator();

	void UnpinBuffer(void* address);
	bool PinBuffer(void *address);
	
	// IUnknown methods
	virtual HRESULT STDMETHODCALLTYPE	QueryInterface(REFIID iid, LPVOID *ppv);
	virtual ULONG STDMETHODCALLTYPE		AddRef(void);
	virtual ULONG STDMETHODCALLTYPE		Release(void);

	// IDeckLinkMemoryAllocator methods
	virtual HRESULT STDMETHODCALLTYPE	AllocateBuffer(unsigned long bufferSize, void* *allocatedBuffer);
	virtual HRESULT STDMETHODCALLTYPE	ReleaseBuffer(void* buffer);
	virtual HRESULT STDMETHODCALLTYPE	Commit();
	virtual HRESULT STDMETHODCALLTYPE	Decommit();

private:
	void Lock()
	{
		pthread_mutex_lock(&mMutex);
	}
	void Unlock()
	{
		pthread_mutex_unlock(&mMutex);
	}

	int									mRefCount;
	pthread_mutex_t						mMutex;
	std::map<void*, u_int>				mAllocatedSize;
	std::set<void *>					mPinnedBuffer;
	std::vector<void*>					mBufferCache;
	u_int								mBufferCacheSize;
};


////////////////////////////////////////////
// Capture Delegate Class
////////////////////////////////////////////

class CaptureDelegate : public IDeckLinkInputCallback
{
	VideoDeckLink*	mpOwner;

public:
	CaptureDelegate(VideoDeckLink* pOwner);

	// IUnknown needs only a dummy implementation
	virtual HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID iid, LPVOID *ppv)	{ return E_NOINTERFACE; }
	virtual ULONG	STDMETHODCALLTYPE	AddRef()								{ return 1; }
	virtual ULONG	STDMETHODCALLTYPE	Release()								{ return 1; }

	virtual HRESULT STDMETHODCALLTYPE	VideoInputFrameArrived(IDeckLinkVideoInputFrame *videoFrame, IDeckLinkAudioInputPacket *audioPacket);
	virtual HRESULT	STDMETHODCALLTYPE	VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags);
};



#endif	/* WITH_DECKLINK */

#endif  /* __VIDEODECKLINK_H__ */
