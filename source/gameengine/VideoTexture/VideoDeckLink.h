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
extern "C" {
#include <pthread.h>
#include "DNA_listBase.h"
#include "BLI_threads.h"
#include "BLI_blenlib.h"
}

#include "DeckLinkAPI.h"
#include "VideoBase.h"


// type VideoDeckLink declaration
class VideoDeckLink : public VideoBase
{
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
};

inline VideoDeckLink *getDeckLink(PyImage *self)
{
	return static_cast<VideoDeckLink*>(self->m_image);
}

#endif	/* WITH_DECKLINK */

#endif  /* __VIDEODECKLINK_H__ */
