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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/stereoimbuf.c
 *  \ingroup imbuf
 */


/* It's become a bit messy... Basically, only the IMB_ prefixed files
 * should remain. */

#include <stddef.h>

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "IMB_allocimbuf.h"
#include "IMB_filetype.h"
#include "IMB_metadata.h"
#include "IMB_colormanagement_intern.h"

#include "imbuf.h"

#include "MEM_guardedalloc.h"

#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"

static void imb_stereo_anaglyph(enum eStereoAnaglyphType UNUSED(mode), ImBuf *left, ImBuf *right, bool is_float, ImBuf *r_ibuf)
{
	int x, y;
	size_t width = r_ibuf->x;
	size_t height= r_ibuf->y;

	const int stride_from = width;
	const int stride_to = width;

	if (is_float){
		const float *rect_left = left->rect_float;
		const float *rect_right= right->rect_float;
		float *rect_to = r_ibuf->rect_float;

		/* always RGBA input */
		for (y = 0; y < height; y++) {
			float *to = rect_to + stride_to * y * 4;
			const float *from_left = rect_left + stride_from * y * 4;
			const float*from_right = rect_right + stride_from * y * 4;

			for (x = 0; x < width; x++, from_left += 4, from_right += 4, to += 4) {
				to[0] = from_left[0];
				to[1] = from_right[1];
				to[2] = from_right[2];
				to[3] = from_right[3];
			}
		}
	}
	else {
		float *rect_left = left->rect_float;
		float *rect_right= right->rect_float;
		uchar *rect_to = (uchar *)r_ibuf->rect;

		/* always RGBA input */
		for (y = 0; y < height; y++) {
			uchar *to = rect_to + stride_to * y * 4;
			float *from_left = rect_left + stride_from * y * 4;
			float *from_right = rect_right + stride_from * y * 4;

			for (x = 0; x < width; x++, from_left += 4, from_right += 4, to += 4) {
				to[0] = FTOCHAR(from_left[0]);
				to[1] = FTOCHAR(from_right[1]);
				to[2] = FTOCHAR(from_right[2]);
				to[3] = FTOCHAR(from_right[3]);
			}
		}
	}
}

static void imb_stereo_dimensions(enum eStereoDisplayMode mode, ImBuf *ibuf, size_t *width, size_t *height)
{
	switch (mode) {
		case S3D_DISPLAY_BLURAY:
		{
			//TODO need to find the dimensions, forgot how big the black bar has to be
			break;
		}
		case S3D_DISPLAY_SIDEBYSIDE:
		{
			*width = ibuf->x * 2;
			*height = ibuf->y;
			break;
		}
		case S3D_DISPLAY_TOPBOTTOM:
		{
			*width = ibuf->x;
			*height = ibuf->y * 2;
			break;
		}
		case S3D_DISPLAY_ANAGLYPH:
		case S3D_DISPLAY_INTERLACE:
		default:
		{
			*width = ibuf->x;
			*height = ibuf->y;
			break;
		}
	}
}

/* left/right are always float */
ImBuf *IMB_stereoImBuf(ImageFormatData *im_format, ImBuf *left, ImBuf *right)
{
	ImBuf *r_ibuf = NULL;
	StereoDisplay *s3d = &im_format->stereo_output;
	eStereoDisplayMode mode = s3d->display_mode;
	size_t width, height;
	const bool is_float = im_format->depth > 8;

	imb_stereo_dimensions(mode, left, &width, &height);

	r_ibuf = IMB_allocImBuf(width, height, left->planes, (is_float ? IB_rectfloat : IB_rect));

	switch (mode) {
		case S3D_DISPLAY_ANAGLYPH:
			imb_stereo_anaglyph(s3d->anaglyph_type, left, right, is_float, r_ibuf);
			break;
		case S3D_DISPLAY_BLURAY:
			break;
		case S3D_DISPLAY_INTERLACE:
			break;
		case S3D_DISPLAY_SIDEBYSIDE:
			break;
		case S3D_DISPLAY_TOPBOTTOM:
			break;
		default:
			break;
	}

	return r_ibuf;
}
