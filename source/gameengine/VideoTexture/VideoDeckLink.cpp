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

#include "MEM_guardedalloc.h"
#include "PIL_time.h"
#include "VideoDeckLink.h"
#include "DeckLink.h"
#include "Exception.h"
#include "KX_KetsjiEngine.h"
#include "KX_PythonInit.h"

extern ExceptionID DeckLinkInternalError;
ExceptionID SourceVideoOnlyCapture, VideoDeckLinkBadFormat, VideoDeckLinkOpenCard, VideoDeckLinkDvpInternalError;
ExpDesc SourceVideoOnlyCaptureDesc(SourceVideoOnlyCapture, "This video source only allows live capture");
ExpDesc VideoDeckLinkBadFormatDesc(VideoDeckLinkBadFormat, "Invalid or unsupported capture format, should be <mode>/<pixel>[/3D]");
ExpDesc VideoDeckLinkOpenCardDesc(VideoDeckLinkOpenCard, "Cannot open capture card, check if driver installed");
ExpDesc VideoDeckLinkDvpInternalErrorDesc(VideoDeckLinkDvpInternalError, "DVP API internal error, please report");


#ifdef WIN32
////////////////////////////////////////////
// SynInfo
//
// Sets up a semaphore which is shared between the GPU and CPU and used to
// synchronise access to DVP buffers.
#define DVP_CHECK(cmd)	if ((cmd) != DVP_STATUS_OK) THRWEXCP(VideoDeckLinkDvpInternalError, S_OK)

struct SyncInfo
{
	SyncInfo(uint32_t semaphoreAllocSize, uint32_t semaphoreAddrAlignment)
	{
		mSemUnaligned = (uint32_t*)malloc(semaphoreAllocSize + semaphoreAddrAlignment - 1);

		// Apply alignment constraints
		uint64_t val = (uint64_t)mSemUnaligned;
		val += semaphoreAddrAlignment - 1;
		val &= ~((uint64_t)semaphoreAddrAlignment - 1);
		mSem = (uint32_t*)val;

		// Initialise
		mSem[0] = 0;
		mReleaseValue = 0;
		mAcquireValue = 0;

		// Setup DVP sync object and import it
		DVPSyncObjectDesc syncObjectDesc;
		syncObjectDesc.externalClientWaitFunc = NULL;
		syncObjectDesc.sem = (uint32_t*)mSem;

		DVP_CHECK(dvpImportSyncObject(&syncObjectDesc, &mDvpSync));

	}
	~SyncInfo()
	{
		dvpFreeSyncObject(mDvpSync);
		free((void*)mSemUnaligned);
	}

	volatile uint32_t*	mSem;
	volatile uint32_t*	mSemUnaligned;
	volatile uint32_t	mReleaseValue;
	volatile uint32_t	mAcquireValue;
	DVPSyncObjectHandle	mDvpSync;
};

////////////////////////////////////////////
// TextureTransferDvp
////////////////////////////////////////////

class TextureTransferDvp : public TextureTransfer
{
public:
	TextureTransferDvp(DVPBufferHandle dvpTextureHandle, TextureDesc *pDesc, void *address)
	{
		DVPSysmemBufferDesc sysMemBuffersDesc;

		if (!mBufferAddrAlignment)
		{
			DVP_CHECK(dvpGetRequiredConstantsGLCtx(&mBufferAddrAlignment, &mBufferGpuStrideAlignment,
				&mSemaphoreAddrAlignment, &mSemaphoreAllocSize,
				&mSemaphorePayloadOffset, &mSemaphorePayloadSize));
		}
		mExtSync = new SyncInfo(mSemaphoreAllocSize, mSemaphoreAddrAlignment);
		mGpuSync = new SyncInfo(mSemaphoreAllocSize, mSemaphoreAddrAlignment);
		sysMemBuffersDesc.width = pDesc->width;
		sysMemBuffersDesc.height = pDesc->height;
		sysMemBuffersDesc.stride = pDesc->stride;
		if (pDesc->format == GL_RED_INTEGER)
		{
			sysMemBuffersDesc.format = DVP_RED_INTEGER;
			sysMemBuffersDesc.type = DVP_UNSIGNED_INT;
		}
		else
		{
			sysMemBuffersDesc.format = DVP_BGRA;
			sysMemBuffersDesc.type = (pDesc->type == GL_UNSIGNED_INT_8_8_8_8) ? DVP_UNSIGNED_INT_8_8_8_8 : DVP_UNSIGNED_INT_8_8_8_8_REV;
		}
		sysMemBuffersDesc.size = pDesc->width * pDesc->height * 4;
		sysMemBuffersDesc.bufAddr = address;
		DVP_CHECK(dvpCreateBuffer(&sysMemBuffersDesc, &mDvpSysMemHandle));
		DVP_CHECK(dvpBindToGLCtx(mDvpSysMemHandle));
		mDvpTextureHandle = dvpTextureHandle;
		mTextureHeight = pDesc->height;
	}
	~TextureTransferDvp()
	{
		dvpUnbindFromGLCtx(mDvpSysMemHandle);
		dvpDestroyBuffer(mDvpSysMemHandle);
		delete mExtSync;
		delete mGpuSync;
	}

	virtual void PerformTransfer()
	{
		// perform the transfer
		// tell DVP that the old texture buffer will no longer be used
		dvpMapBufferEndAPI(mDvpTextureHandle);
		// do we need this?
		mGpuSync->mReleaseValue++;
		dvpBegin();
		// Copy from system memory to GPU texture
		dvpMapBufferWaitDVP(mDvpTextureHandle);
		dvpMemcpyLined(mDvpSysMemHandle, mExtSync->mDvpSync, mExtSync->mAcquireValue, DVP_TIMEOUT_IGNORED,
			mDvpTextureHandle, mGpuSync->mDvpSync, mGpuSync->mReleaseValue, 0, mTextureHeight);
		dvpMapBufferEndDVP(mDvpTextureHandle);
		dvpEnd();
		dvpMapBufferWaitAPI(mDvpTextureHandle);
		// the transfer is now complete and the texture is ready for use
	}

private:
	static uint32_t			mBufferAddrAlignment;
	static uint32_t			mBufferGpuStrideAlignment;
	static uint32_t			mSemaphoreAddrAlignment;
	static uint32_t			mSemaphoreAllocSize;
	static uint32_t			mSemaphorePayloadOffset;
	static uint32_t			mSemaphorePayloadSize;

	SyncInfo*				mExtSync;
	SyncInfo*				mGpuSync;
	DVPBufferHandle			mDvpSysMemHandle;
	DVPBufferHandle			mDvpTextureHandle;
	u_int					mTextureHeight;
};

uint32_t	TextureTransferDvp::mBufferAddrAlignment;
uint32_t	TextureTransferDvp::mBufferGpuStrideAlignment;
uint32_t	TextureTransferDvp::mSemaphoreAddrAlignment;
uint32_t	TextureTransferDvp::mSemaphoreAllocSize;
uint32_t	TextureTransferDvp::mSemaphorePayloadOffset;
uint32_t	TextureTransferDvp::mSemaphorePayloadSize;

#endif



////////////////////////////////////////////
// PinnedMemoryAllocator
////////////////////////////////////////////


// static members
bool		PinnedMemoryAllocator::mGPUDirectInitialized = false;
bool		PinnedMemoryAllocator::mHasDvp = false;
bool		PinnedMemoryAllocator::mHasAMDPinnedMemory = false;
size_t		PinnedMemoryAllocator::mReservedProcessMemory = 0;

bool PinnedMemoryAllocator::ReserveMemory(size_t size)
{
#ifdef WIN32
	// Increase the process working set size to allow pinning of memory.
	if (size <= mReservedProcessMemory)
		return true;
	SIZE_T dwMin = 0, dwMax = 0;
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, GetCurrentProcessId());
	if (!hProcess)
		return false;

	// Retrieve the working set size of the process.
	if (!dwMin && !GetProcessWorkingSetSize(hProcess, &dwMin, &dwMax))
		return false;

	BOOL res = SetProcessWorkingSetSize(hProcess, (size - mReservedProcessMemory) + dwMin, (size - mReservedProcessMemory) + dwMax);
	if (!res)
		return false;
	mReservedProcessMemory = size;
	CloseHandle(hProcess);
#endif
	return true;
}

PinnedMemoryAllocator::PinnedMemoryAllocator(unsigned cacheSize, size_t memSize) :
mRefCount(1),
mBufferCacheSize(cacheSize),
mUnpinnedTextureBuffer(0)
#ifdef WIN32
, mDvpCaptureTextureHandle(0)
#endif
{
	pthread_mutex_init(&mMutex, NULL);
	// do it once
	if (!mGPUDirectInitialized)
	{
#ifdef WIN32
		// In windows, AMD_pinned_memory option is not available, 
		// we must use special DVP API only available for Quadro cards
		const char* renderer = (const char *)glGetString(GL_RENDERER);
		mHasDvp = (strstr(renderer, "Quadro") != NULL);

		if (mHasDvp)
		{
			DVP_CHECK(dvpInitGLContext(DVP_DEVICE_FLAGS_SHARE_APP_CONTEXT));
		}
#endif
		if (GLEW_AMD_pinned_memory)
			mHasAMDPinnedMemory = true;

		mGPUDirectInitialized = true;
	}
	if (!mHasDvp && !mHasAMDPinnedMemory)
	{
		// if we cannot use GPUDirect, we will send the video to the GPU using OGL buffer
		glGenBuffers(1, &mUnpinnedTextureBuffer);
	}
	else
	{
		ReserveMemory(memSize);
	}
}

PinnedMemoryAllocator::~PinnedMemoryAllocator()
{
	void *address;
	// first clean the cache if not already done
	while (!mBufferCache.empty())
	{
		address = mBufferCache.back();
		mBufferCache.pop_back();
		_ReleaseBuffer(address);
	}
	// clean preallocated buffers
	while (!mAllocatedSize.empty())
	{
		address = mAllocatedSize.begin()->first;
		_ReleaseBuffer(address);
	}

	if (mUnpinnedTextureBuffer)
		glDeleteBuffers(1, &mUnpinnedTextureBuffer);
#ifdef WIN32
	if (mDvpCaptureTextureHandle)
		dvpDestroyBuffer(mDvpCaptureTextureHandle);
#endif
}

bool PinnedMemoryAllocator::_PinBuffer(void *address, u_int size)
{
#ifdef WIN32
	return VirtualLock(address, size);
#else
	// Linux doesn't have the equivalent?
	return true;
#endif
}

void PinnedMemoryAllocator::_UnpinBuffer(void* address, u_int size)
{
#ifdef WIN32
	VirtualUnlock(address, size);
#endif
}

void PinnedMemoryAllocator::TransferBuffer(void* address, TextureDesc* texDesc, GLuint texId)
{
	u_int allocatedSize = 0;
	TextureTransfer *pTransfer = NULL;

	Lock();
	if (mAllocatedSize.count(address) > 0)
		allocatedSize = mAllocatedSize[address];
	Unlock();
	if (!allocatedSize)
		// internal error!!
		return;
#ifdef WIN32
	if (mHasDvp)
	{
		if (!mDvpCaptureTextureHandle)
		{
			// first time we try to send data to the GPU, allocate a buffer for the texture
			glBindTexture(GL_TEXTURE_2D, texId);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexImage2D(GL_TEXTURE_2D, 0, ((texDesc->format == GL_RED_INTEGER) ? GL_R32UI : GL_RGBA), texDesc->width, texDesc->height, 0, texDesc->format, texDesc->type, NULL);
			// bind DVP to the OGL texture
			DVP_CHECK(dvpCreateGPUTextureGL(texId, &mDvpCaptureTextureHandle));
		}
		Lock();
		if (mPinnedBuffer.count(address) > 0)
		{
			pTransfer = mPinnedBuffer[address];
		}
		Unlock();
		if (!pTransfer)
		{
			if (!_PinBuffer(address, allocatedSize))
				return;
			pTransfer = new TextureTransferDvp(mDvpCaptureTextureHandle, texDesc, address);
			if (pTransfer)
			{
				Lock();
				mPinnedBuffer[address] = pTransfer;
				Unlock();
			}
		}
	}
	else
#endif
	if (mHasAMDPinnedMemory)
	{
		pTransfer = NULL;
	}
	else
	{
		pTransfer = NULL;
	}
	if (pTransfer)
		pTransfer->PerformTransfer();
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
HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::AllocateBuffer(dl_size_t bufferSize, void* *allocatedBuffer)
{
	Lock();
	if (mBufferCache.empty())
	{
		// Allocate memory on a page boundary
		// Note: aligned alloc exist in Blender but only for small alignment, use direct allocation then.
		// Note: the DeckLink API tries to allocate up to 65 buffer in advance, we will limit this to 3
		//       because we don't need any caching
		if (mAllocatedSize.size() >= 5)
			*allocatedBuffer = NULL;
		else 
		{
#ifdef WIN32
			*allocatedBuffer = VirtualAlloc(NULL, bufferSize, MEM_COMMIT | MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE);
#else
			if (posix_memalign(allocatedBuffer, 4096, bufferSize) != 0)
				*allocatedBuffer = NULL;
#endif
			mAllocatedSize[*allocatedBuffer] = bufferSize;
		}
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

HRESULT STDMETHODCALLTYPE PinnedMemoryAllocator::ReleaseBuffer(void* buffer)
{
	HRESULT result = S_OK;
	Lock();
	if (mBufferCache.size() < mBufferCacheSize)
	{
		mBufferCache.push_back(buffer);
	}
	else
	{
		result = _ReleaseBuffer(buffer);
	}
	Unlock();
	return result;
}


HRESULT PinnedMemoryAllocator::_ReleaseBuffer(void* buffer)
{
	u_int allocatedSize = 0;
	TextureTransfer *pTransfer;
	if (mAllocatedSize.count(buffer) == 0)
	{
		// Internal error!!
		return S_OK;
	}
	else
	{
		// No room left in cache, so un-pin (if it was pinned) and free this buffer
		allocatedSize = mAllocatedSize[buffer];
		if (mPinnedBuffer.count(buffer) > 0)
		{
			_UnpinBuffer(buffer, allocatedSize);
			pTransfer = mPinnedBuffer[buffer];
			mPinnedBuffer.erase(buffer);
			delete pTransfer;
		}
#ifdef WIN32
		VirtualFree(buffer, 0, MEM_RELEASE);
#else
		free(buffer);
#endif
		mAllocatedSize.erase(buffer);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::Commit()
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::Decommit()
{
	void *buffer;
	Lock();
	while (!mBufferCache.empty())
	{
		// Cleanup any frames allocated and pinned in AllocateBuffer() but not freed in ReleaseBuffer()
		buffer = mBufferCache.back();
		mBufferCache.pop_back();
		_ReleaseBuffer(buffer);
	}
	Unlock();
	return S_OK;
}


////////////////////////////////////////////
// Capture Delegate Class
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




// macro for exception handling and logging
#define CATCH_EXCP catch (Exception & exp) \
{ exp.report(); m_status = SourceError; }

// class VideoDeckLink


// constructor
VideoDeckLink::VideoDeckLink (HRESULT * hRslt) : VideoBase(),
mDLInput(NULL),
mUse3D(false),
mFrameWidth(0),
mFrameHeight(0),
mpAllocator(NULL),
mpCaptureDelegate(NULL),
mpCacheFrame(NULL),
mClosing(false)
{
	mDisplayMode = (BMDDisplayMode)0;
	mPixelFormat = (BMDPixelFormat)0;
	pthread_mutex_init(&mCacheMutex, NULL);
}

// destructor
VideoDeckLink::~VideoDeckLink ()
{
	LockCache();
	mClosing = true;
	if (mpCacheFrame)
	{
		mpCacheFrame->Release();
		mpCacheFrame = NULL;
	}
	UnlockCache();
	if (mDLInput != NULL)
	{
		// Cleanup for Capture
		mDLInput->StopStreams();
		mDLInput->SetCallback(NULL);
		mDLInput->DisableVideoInput();
		mDLInput->FlushStreams();
		if (mDLInput->Release() != 0)
			THRWEXCP(DeckLinkInternalError, S_OK);
		mDLInput = NULL;
	}
	
	if (mpAllocator)
	{
		// if the device was properly cleared, this should be 0
		if (mpAllocator->Release() != 0)
			THRWEXCP(DeckLinkInternalError, S_OK);
		mpAllocator = NULL;
	}
	if (mpCaptureDelegate)
	{
		delete mpCaptureDelegate;
		mpCaptureDelegate = NULL;
	}
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
	// throws if bad mode
	decklink_ReadDisplayMode(format, len, &mDisplayMode);
	// skip /
	pPixel++;
	len = ((mUse3D) ? (size_t)(p3D - pPixel) : strlen(pPixel));
	// throws if bad format
	decklink_ReadPixelFormat(pPixel, len, &mPixelFormat);

	// Caution: DeckLink API used from this point, make sure entity are released before throwing
	// open the card
	pIterator = BMD_CreateDeckLinkIterator();
	if (pIterator) 
	{
		i = 0;
		while (pIterator->Next(&pDL) == S_OK) 
		{
			if (i == camIdx) 
			{
				if (pDL->QueryInterface(IID_IDeckLinkInput, (void**)&mDLInput) != S_OK)
					mDLInput = NULL;
				pDL->Release();
				break;
			}
			i++;
			pDL->Release();
		}
		pIterator->Release();
	}
	if (!mDLInput)
		THRWEXCP(VideoDeckLinkOpenCard, S_OK);

	
	// check if display mode and pixel format are supported
	if (mDLInput->GetDisplayModeIterator(&pDLDisplayModeIterator) != S_OK)
		THRWEXCP(DeckLinkInternalError, S_OK);

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
	mTextureDesc.height = (mUse3D) ? 2 * mFrameHeight : mFrameHeight;
	pDLDisplayMode->GetFrameRate(&frameDuration, &frameTimescale);
	pDLDisplayMode->Release();
	// for information, in case the application wants to know
	m_size[0] = mFrameWidth;
	m_size[1] = mTextureDesc.height;
	m_frameRate = 1.0f+(float)frameTimescale / (float)frameDuration;

	switch (mPixelFormat)
	{
	case bmdFormat8BitYUV:
		// 2 pixels per word
		mTextureDesc.stride = mFrameWidth * 2;
		mTextureDesc.width = mFrameWidth / 2;
		mTextureDesc.format = GL_RED_INTEGER;
		mTextureDesc.type = GL_UNSIGNED_INT;
		break;
	case bmdFormat10BitYUV:
		// 6 pixels in 4 words, rounded to 48 pixels
		mTextureDesc.stride = ((mFrameWidth + 47) / 48) * 128;
		mTextureDesc.width = ((mFrameWidth + 5) / 6) * 4;
		mTextureDesc.format = GL_RED_INTEGER;
		mTextureDesc.type = GL_UNSIGNED_INT;
		break;
	case bmdFormat8BitARGB:
		mTextureDesc.stride = mFrameWidth * 4;
		mTextureDesc.width = mFrameWidth;
		mTextureDesc.format = GL_BGRA;
		mTextureDesc.type = GL_UNSIGNED_INT_8_8_8_8;
		break;
	case bmdFormat8BitBGRA:
		mTextureDesc.stride = mFrameWidth * 4;
		mTextureDesc.width = mFrameWidth;
		mTextureDesc.format = GL_BGRA;
		mTextureDesc.type = GL_UNSIGNED_INT_8_8_8_8_REV;
		break;
	case bmdFormat10BitRGBXLE:
	case bmdFormat10BitRGBX:
	case bmdFormat10BitRGB:
		mTextureDesc.stride = ((mFrameWidth + 63) / 64) * 256;
		mTextureDesc.width = mFrameWidth;
		mTextureDesc.format = GL_RED_INTEGER;
		mTextureDesc.type = GL_UNSIGNED_INT;
		break;
	case bmdFormat12BitRGB:
	case bmdFormat12BitRGBLE:
		mTextureDesc.stride = (mFrameWidth * 36) / 8;
		mTextureDesc.width = (mFrameWidth / 8) * 9;
		mTextureDesc.format = GL_RED_INTEGER;
		mTextureDesc.type = GL_UNSIGNED_INT;
		break;
	default:
		// for unknown pixel format, this will be resolved when a frame arrives
		mTextureDesc.format = GL_RED_INTEGER;
		mTextureDesc.type = GL_UNSIGNED_INT;
		break;
	}
	// check that the pixel format is consistent with display mode (no padding)
	if (mTextureDesc.stride)
	{
		// each OGL pixel is 4 bytes (either RGBA or RED_INTEGER)
		// stride is the size of a row in bytes
		// make sure there is no padding
		if (mTextureDesc.stride != mTextureDesc.width * 4)
			THRWEXCP(VideoDeckLinkBadFormat, S_OK);
	}

	// custom allocator, 3 frame in cache should be enough
	// make sure we allow up to 10 frame in memory for pinning
	// note: some pixel format take more than 4 bytes but the difference is small (9/8 versus 1)
	mpAllocator = new PinnedMemoryAllocator(3, mFrameWidth*mTextureDesc.height * 4 * 10);

	if (mDLInput->SetVideoInputFrameMemoryAllocator(mpAllocator) != S_OK)
		THRWEXCP(DeckLinkInternalError, S_OK);

	if (mDLInput->EnableVideoInput(mDisplayMode, mPixelFormat, ((mUse3D) ? bmdVideoInputDualStream3D : bmdVideoInputFlagDefault)) != S_OK)
		// this shouldn't failed, we tested above
		THRWEXCP(DeckLinkInternalError, S_OK); 

	mpCaptureDelegate = new CaptureDelegate(this);
	if (mDLInput->SetCallback(mpCaptureDelegate) != S_OK)
		THRWEXCP(DeckLinkInternalError, S_OK);

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
		// BUG: the dvpBindToGLCtx function fails the first time it is used, don't know why.
		// This causes an exception to be thrown.
		// This should be fixed but in the meantime we will catch the exception because
		// it is crucial that we release the frame to keep the reference count right on the DeckLink device
		try
		{
			u_int rowSize = pFrame->GetRowBytes();
			u_int textureSize = rowSize * pFrame->GetHeight();
			u_int expectedSize;
			void* videoPixels = NULL;
			void* rightEyePixels = NULL;
			if (!mTextureDesc.stride)
			{
				// we could not compute the texture size earlier (unknown pixel size)
				// let's do it now
				mTextureDesc.stride = rowSize;
				mTextureDesc.width = mTextureDesc.stride / 4;
			}
			if (mTextureDesc.stride != rowSize)
			{
				// unexpected frame size, ignore
				// TBD: print a warning
			}
			else
			{
				pFrame->GetBytes(&videoPixels);
				if (mUse3D) {
					IDeckLinkVideoFrame3DExtensions *if3DExtensions = NULL;
					IDeckLinkVideoFrame *rightEyeFrame = NULL;
                    if (pFrame->QueryInterface(IID_IDeckLinkVideoFrame3DExtensions, (void **)&if3DExtensions) == S_OK &&
						if3DExtensions->GetFrameForRightEye(&rightEyeFrame) == S_OK) {
						rightEyeFrame->GetBytes(&rightEyePixels);
						textureSize += ((uint64_t)rightEyePixels - (uint64_t)videoPixels);
					}
					if (rightEyeFrame)
						rightEyeFrame->Release();
					if (if3DExtensions)
						if3DExtensions->Release();
				}
				expectedSize = mTextureDesc.width * mTextureDesc.height * 4;
				if (expectedSize == textureSize)
				{
					// this means that both left and right frame are contiguous and that there is no padding
					// do the transfer
					mpAllocator->TransferBuffer(videoPixels, &mTextureDesc, texId);
				}
			}
		} 
		catch (Exception &)
		{
			pFrame->Release();
			throw;
		}
		// this will trigger PinnedMemoryAllocator::RealaseBuffer
		pFrame->Release();
	}
	// currently we don't pass the image to the application
	m_avail = false;
}

// A frame is available from the board
// Called from an internal thread, just pass the frame to the main thread
void VideoDeckLink::VideoFrameArrived(IDeckLinkVideoInputFrame* inputFrame)
{
	IDeckLinkVideoInputFrame* pOldFrame = NULL;
	LockCache();
	if (!mClosing)
	{
		pOldFrame = mpCacheFrame;
		mpCacheFrame = inputFrame;
		inputFrame->AddRef();
	}
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
// DeckLink Capture Delegate Class
////////////////////////////////////////////

#endif		// WITH_DECKLINK

