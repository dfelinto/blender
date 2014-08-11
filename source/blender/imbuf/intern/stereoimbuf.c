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

static void imb_stereo_anaglyph(enum eStereoAnaglyphType mode, ImBuf *left, ImBuf *right, bool is_float, ImBuf *r_ibuf)
{
	int x, y;
	size_t width = r_ibuf->x;
	size_t height= r_ibuf->y;

	const int stride_from = width;
	const int stride_to = width;

	int anaglyph_encoding[3][3] = {
	    {0,1,1},
	    {1,0,1},
	    {0,0,1},
	};

	int r, g, b;

	r = anaglyph_encoding[mode][0];
	g = anaglyph_encoding[mode][1];
	b = anaglyph_encoding[mode][2];

	if (is_float){
		float *rect_to = r_ibuf->rect_float;
		const float *rect_left = left->rect_float;
		const float *rect_right= right->rect_float;

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			float *to = rect_to + stride_to * y * 4;
			const float *from[2] = {
			          rect_left + stride_from * y * 4,
			          rect_right + stride_from * y * 4,
			};

			for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
				to[0] = from[r][0];
				to[1] = from[g][1];
				to[2] = from[b][2];
				to[3] = MAX2(from[0][2], from[0][2]);
			}
		}
	}
	else {
		uchar *rect_to = (uchar *)r_ibuf->rect;
		const uchar *rect_left = (uchar *)left->rect;
		const uchar *rect_right= (uchar *)right->rect;

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			uchar *to = rect_to + stride_to * y * 4;
			uchar *from[2] = {
			    rect_left + stride_from * y * 4,
			    rect_right + stride_from * y * 4,
			};

			for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
				to[0] = from[r][0];
				to[1] = from[g][1];
				to[2] = from[b][2];
				to[3] = MAX2(from[0][2], from[0][2]);
			}
		}
	}
}

static void imb_stereo_interlace(enum eStereoInterlaceType mode, const bool swap, ImBuf *left, ImBuf *right, bool is_float, ImBuf *r_ibuf)
{
	int x, y;
	size_t width = r_ibuf->x;
	size_t height= r_ibuf->y;

	const int stride_from = width;
	const int stride_to = width;

	if (is_float){
		float *rect_to = r_ibuf->rect_float;
		const float *rect_left = left->rect_float;
		const float *rect_right= right->rect_float;

		switch(mode) {
			case S3D_INTERLACE_ROW:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					float *to = rect_to + stride_to * y * 4;
					const float *from[2] = {
					      rect_left + stride_from * y * 4,
					      rect_right + stride_from * y * 4,
					};
					memcpy(to, from[i], sizeof(float) * 4 * stride_from);
					i = !i;
				}
				break;
			}
			case S3D_INTERLACE_COLUMN:
			{
				for (y = 0; y < height; y++) {
					float *to = rect_to + stride_to * y * 4;
					const float *from[2] = {
					      rect_left + stride_from * y * 4,
					      rect_right + stride_from * y * 4,
					};

					char i = (char) swap;
					for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
						copy_v4_v4(to, from[i]);
						i = !i;
					}
				}
				break;
			}
			case S3D_INTERLACE_CHECKERBOARD:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					float *to = rect_to + stride_to * y * 4;
					const float *from[2] = {
					      rect_left + stride_from * y * 4,
					      rect_right + stride_from * y * 4,
					};
					char j = i;
					for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
						copy_v4_v4(to, from[j]);
						j = !j;
					}
					i = !i;
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
	else {
		uchar *rect_to = (uchar *)r_ibuf->rect;
		const uchar *rect_left = (uchar *)left->rect;
		const uchar *rect_right= (uchar *)right->rect;

		switch(mode) {
			case S3D_INTERLACE_ROW:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					uchar *to = rect_to + stride_to * y * 4;
					const uchar *from[2] = {
					      rect_left + stride_from * y * 4,
					      rect_right + stride_from * y * 4,
					      };
					memcpy(to, from[i], sizeof(uchar) * 4 * stride_from);
					i = !i;
				}
				break;
			}
			case S3D_INTERLACE_COLUMN:
			{
				for (y = 0; y < height; y++) {
					uchar *to = rect_to + stride_to * y * 4;
					const uchar *from[2] = {
					      rect_left + stride_from * y * 4,
					      rect_right + stride_from * y * 4,
					};
					char i = (char) swap;
					for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
						copy_v4_v4_uchar(to, from[i]);
						i = !i;
					}
				}
				break;
			}
			case S3D_INTERLACE_CHECKERBOARD:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					uchar *to = rect_to + stride_to * y * 4;
					const uchar *from[2] = {
					      rect_left + stride_from * y * 4,
					      rect_right + stride_from * y * 4,
					};
					char j = i;
					for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
						copy_v4_v4_uchar(to, from[j]);
						j = !j;
					}
					i = !i;
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

static void imb_stereo_sidebyside(const bool crosseyed, ImBuf *left, ImBuf *right, bool is_float, ImBuf *r_ibuf)
{
	int y;
	size_t width = r_ibuf->x / 2;
	size_t height= r_ibuf->y;

	const int stride_from = width;
	const int stride_to = width * 2;

	const int l =  (int) crosseyed;
	const int r = !l;

	BLI_assert((left->x == right->x) && (left->x == width));

	if (is_float){
		float *rect_to = r_ibuf->rect_float;
		const float *rect_left = left->rect_float;
		const float *rect_right= right->rect_float;

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			float *to = rect_to + stride_to * y * 4;
			float *from[2] = {
			    rect_left + stride_from * y * 4,
			    rect_right + stride_from * y * 4,
			};

			memcpy(to, from[l], sizeof(float) * 4 * stride_from);
			memcpy(to + 4 * stride_from, from[r], sizeof(float) * 4 * stride_from);
		}
	}
	else {
		uchar *rect_to = (uchar *)r_ibuf->rect;
		const uchar *rect_left = (uchar *)left->rect;
		const uchar *rect_right= (uchar *)right->rect;

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			uchar *to = rect_to + stride_to * y * 4;
			uchar *from[2] = {
			    rect_left + stride_from * y * 4,
			    rect_right + stride_from * y * 4,
			};

			memcpy(to, from[l], sizeof(uchar) * 4 * stride_from);
			memcpy(to + 4 * stride_from, from[r], sizeof(uchar) * 4 * stride_from);
		}
	}
}

static void imb_stereo_topbottom(ImBuf *left, ImBuf *right, bool is_float, ImBuf *r_ibuf)
{
	int y;
	size_t width = r_ibuf->x;
	size_t height= r_ibuf->y / 2;

	const int stride_from = width;
	const int stride_to = width;

	BLI_assert((left->y == right->y) && (left->y == height));

	if (is_float){
		float *rect_to = r_ibuf->rect_float;
		const float *rect_left = left->rect_float;
		const float *rect_right= right->rect_float;

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			float *to = rect_to + stride_to * y * 4;
			float *from[2] = {
			    rect_left + stride_from * y * 4,
			    rect_right + stride_from * y * 4,
			};

			memcpy(to, from[1], sizeof(float) * 4 * stride_from);
			memcpy(to + 4 * height * stride_to, from[0], sizeof(float) * 4 * stride_from);
		}
	}
	else {
		uchar *rect_to = (uchar *)r_ibuf->rect;
		const uchar *rect_left = (uchar *)left->rect;
		const uchar *rect_right= (uchar *)right->rect;

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			uchar *to = rect_to + stride_to * y * 4;
			uchar *from[2] = {
			    rect_left + stride_from * y * 4,
				rect_right + stride_from * y * 4,
			};

			memcpy(to, from[1], sizeof(uchar) * 4 * stride_from);
			memcpy(to + 4 * height * stride_to, from[0], sizeof(uchar) * 4 * stride_from);
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
			imb_stereo_interlace(s3d->interlace_type, (s3d->flag & S3D_INTERLACE_SWAP), left, right, is_float, r_ibuf);
			break;
		case S3D_DISPLAY_SIDEBYSIDE:
			imb_stereo_sidebyside((s3d->flag & S3D_SIDEBYSIDE_CROSSEYED), left, right, is_float, r_ibuf);
			break;
		case S3D_DISPLAY_TOPBOTTOM:
			imb_stereo_topbottom(left, right, is_float, r_ibuf);
			break;
		default:
			break;
	}

	return r_ibuf;
}
