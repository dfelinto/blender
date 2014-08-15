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

/* prototypes */
struct Stereo3DData;
static void imb_stereo_doit(struct Stereo3DData *s3d_data, struct StereoDisplay *s3d);

typedef struct Stereo3DData {
	float *rectf[3];
	uchar *rect[3];
	size_t x, y, channels;
	bool is_float;
} Stereo3DData;

static void imb_stereo_anaglyph(Stereo3DData *s3d, enum eStereoAnaglyphType mode)
{
	int x, y;
	size_t width = s3d->x;
	size_t height= s3d->y;
	const size_t channels = s3d->channels;

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

	if (s3d->is_float){
		float *rect_to = s3d->rectf[2];
		float *rect_left = s3d->rectf[0];
		float *rect_right= s3d->rectf[1];

		if (channels == 3) {
			for (y = 0; y < height; y++) {
				float *to = rect_to + stride_to * y * 3;
				float *from[2] = {
				        rect_left + stride_from * y * 3,
				        rect_right + stride_from * y * 3,
				};

				for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
					to[0] = from[r][0];
					to[1] = from[g][1];
					to[2] = from[b][2];
				}
			}
		}
		else if (channels == 4) {
			for (y = 0; y < height; y++) {
				float *to = rect_to + stride_to * y * 4;
				float *from[2] = {
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
	else {
		uchar *rect_to = s3d->rect[2];
		uchar *rect_left = s3d->rect[0];
		uchar *rect_right= s3d->rect[1];

		if (channels == 3) {
			for (y = 0; y < height; y++) {
				uchar *to = rect_to + stride_to * y * 3;
				uchar *from[2] = {
				        rect_left + stride_from * y * 3,
				        rect_right + stride_from * y * 3,
				};

				for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
					to[0] = from[r][0];
					to[1] = from[g][1];
					to[2] = from[b][2];
				}
			}
		}
		else if (channels == 4) {
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
}

static void imb_stereo_interlace(Stereo3DData *s3d, enum eStereoInterlaceType mode, const bool swap)
{
	int x, y;
	size_t width = s3d->x;
	size_t height= s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width;

	if (s3d->is_float){
		float *rect_to = s3d->rectf[2];
		const float *rect_left = s3d->rectf[0];
		const float *rect_right= s3d->rectf[1];

		switch(mode) {
			case S3D_INTERLACE_ROW:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					float *to = rect_to + stride_to * y * channels;
					const float *from[2] = {
					      rect_left + stride_from * y * channels,
					      rect_right + stride_from * y * channels,
					};
					memcpy(to, from[i], sizeof(float) * channels * stride_from);
					i = !i;
				}
				break;
			}
			case S3D_INTERLACE_COLUMN:
			{
				if (channels == 1) {
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y;
						const float *from[2] = {
						      rect_left + stride_from * y,
						      rect_right + stride_from * y,
						};

						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 1, from[1] += 1, to += 1) {
							to[0] = from[i][0];
							i = !i;
						}
					}
				}
				else if (channels == 3) {
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y * 3;
						const float *from[2] = {
						      rect_left + stride_from * y * 3,
						      rect_right + stride_from * y * 3,
						};

						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
							copy_v3_v3(to, from[i]);
							i = !i;
						}
					}
				}
				else if (channels == 4) {
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y * channels;
						const float *from[2] = {
						      rect_left + stride_from * y * channels,
						      rect_right + stride_from * y * channels,
						};

						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
							copy_v4_v4(to, from[i]);
							i = !i;
						}
					}
				}
				break;
			}
			case S3D_INTERLACE_CHECKERBOARD:
			{
				if (channels == 1) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y;
						const float *from[2] = {
						        rect_left + stride_from * y,
						        rect_right + stride_from * y,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 1, from[1] += 1, to += 1) {
							to[0] = from[j][0];
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 3) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y * 3;
						const float *from[2] = {
						        rect_left + stride_from * y * 3,
						        rect_right + stride_from * y * 3,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
							copy_v3_v3(to, from[j]);
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 4) {
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
		uchar *rect_to = s3d->rect[2];
		const uchar *rect_left = s3d->rect[0];
		const uchar *rect_right= s3d->rect[1];

		switch(mode) {
			case S3D_INTERLACE_ROW:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					uchar *to = rect_to + stride_to * y * channels;
					const uchar *from[2] = {
					      rect_left + stride_from * y * channels,
					      rect_right + stride_from * y * channels,
					      };
					memcpy(to, from[i], sizeof(uchar) * channels * stride_from);
					i = !i;
				}
				break;
			}
			case S3D_INTERLACE_COLUMN:
			{
				if (channels == 1) {
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y;
						const uchar *from[2] = {
						        rect_left + stride_from * y,
						        rect_right + stride_from * y,
						};
						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 1, from[1] += 1, to += 1) {
							to[0] = from[i][0];
							i = !i;
						}
					}
				}
				else if (channels == 3) {
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y * 3;
						const uchar *from[2] = {
						        rect_left + stride_from * y * 3,
						        rect_right + stride_from * y * 3,
						};
						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
							copy_v3_v3_uchar(to, from[i]);
							i = !i;
						}
					}
				}
				else if (channels == 4) {
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
				}
				break;
			}
			case S3D_INTERLACE_CHECKERBOARD:
			{
				if (channels == 1) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y;
						const uchar *from[2] = {
						        rect_left + stride_from * y,
						        rect_right + stride_from * y,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 1, from[1] += 1, to += 1) {
							to[0] = from[j][0];
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 3) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y * 3;
						const uchar *from[2] = {
						        rect_left + stride_from * y * 3,
						        rect_right + stride_from * y * 3,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
							copy_v3_v3_uchar(to, from[j]);
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 4) {
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

static void imb_stereo_sidebyside(Stereo3DData *s3d, const bool crosseyed)
{
	int y;
	size_t width = s3d->x;
	size_t height= s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width * 2;

	const int l =  (int) crosseyed;
	const int r = !l;

	if (s3d->is_float){
		float *rect_to = s3d->rectf[2];
		float *rect_left = s3d->rectf[0];
		float *rect_right= s3d->rectf[1];

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			float *to = rect_to + stride_to * y * channels;
			float *from[2] = {
			    rect_left + stride_from * y * channels,
			    rect_right + stride_from * y * channels,
			};

			memcpy(to, from[l], sizeof(float) * channels * stride_from);
			memcpy(to + channels * stride_from, from[r], sizeof(float) * channels * stride_from);
		}
	}
	else {
		uchar *rect_to = s3d->rect[2];
		uchar *rect_left = s3d->rect[0];
		uchar *rect_right= s3d->rect[1];

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			uchar *to = rect_to + stride_to * y * channels;
			uchar *from[2] = {
			    rect_left + stride_from * y * channels,
			    rect_right + stride_from * y * channels,
			};

			memcpy(to, from[l], sizeof(uchar) * channels * stride_from);
			memcpy(to + channels * stride_from, from[r], sizeof(uchar) * channels * stride_from);
		}
	}
}

static void imb_stereo_topbottom(Stereo3DData *s3d)
{
	int y;
	size_t width = s3d->x;
	size_t height= s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width;

	if (s3d->is_float){
		float *rect_to = s3d->rectf[2];
		float *rect_left = s3d->rectf[0];
		float *rect_right= s3d->rectf[1];

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			float *to = rect_to + stride_to * y * channels;
			float *from[2] = {
			    rect_left + stride_from * y * channels,
			    rect_right + stride_from * y * channels,
			};

			memcpy(to, from[1], sizeof(float) * channels * stride_from);
			memcpy(to + channels * height * stride_to, from[0], sizeof(float) * channels * stride_from);
		}
	}
	else {
		uchar *rect_to = s3d->rect[2];
		uchar *rect_left = s3d->rect[0];
		uchar *rect_right= s3d->rect[1];

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			uchar *to = rect_to + stride_to * y * channels;
			uchar *from[2] = {
			    rect_left + stride_from * y * channels,
				rect_right + stride_from * y * channels,
			};

			memcpy(to, from[1], sizeof(uchar) * channels * stride_from);
			memcpy(to + channels * height * stride_to, from[0], sizeof(uchar) * channels * stride_from);
		}
	}
}

void IMB_stereo_dimensions(const char mode, const size_t width, const size_t height, size_t *r_width, size_t *r_height)
{
	switch (mode) {
		case S3D_DISPLAY_BLURAY:
		{
			//TODO need to find the dimensions, forgot how big the black bar has to be
			break;
		}
		case S3D_DISPLAY_SIDEBYSIDE:
		{
			*r_width = width * 2;
			*r_height = height;
			break;
		}
		case S3D_DISPLAY_TOPBOTTOM:
		{
			*r_width = width;
			*r_height = height * 2;
			break;
		}
		case S3D_DISPLAY_ANAGLYPH:
		case S3D_DISPLAY_INTERLACE:
		default:
		{
			*r_width = width;
			*r_height = height;
			break;
		}
	}
}

static void imb_stereo_data_initialize(Stereo3DData *s3d_data, ImageFormatData *im_format,
                                       const size_t x, const size_t y, const size_t channels,
                                       int *left_rect, int *right_rect, int *r_rect,
                                       float *left_rectf, float *right_rectf, float *r_rectf)
{
	s3d_data->is_float = im_format->depth > 8;
	s3d_data->x = x;
	s3d_data->y = y;
	s3d_data->channels = channels;
	s3d_data->rect[0] = (uchar *)left_rect;
	s3d_data->rect[1] = (uchar *)right_rect;
	s3d_data->rect[2] = (uchar *)r_rect;
	s3d_data->rectf[0] = left_rectf;
	s3d_data->rectf[1] = right_rectf;
	s3d_data->rectf[2] = r_rectf;
}

int *IMB_stereo_from_rect(ImageFormatData *im_format, const size_t x, const size_t y, const size_t channels, int *left, int *right)
{
	int *r_rect;
	Stereo3DData s3d_data = {{NULL}};
	size_t width, height;

	IMB_stereo_dimensions(im_format->stereo_output.display_mode, x, y, &width, &height);
	r_rect = MEM_mallocN(channels * sizeof(int) * width * height, __func__);

	imb_stereo_data_initialize(&s3d_data, im_format, x, y, channels, left, right, r_rect, NULL, NULL, NULL);
	imb_stereo_doit(&s3d_data, &im_format->stereo_output);

	return r_rect;
}

float *IMB_stereo_from_rectf(ImageFormatData *im_format, const size_t x, const size_t y, const size_t channels, float *left, float *right)
{
	float *r_rectf;
	Stereo3DData s3d_data = {{NULL}};
	size_t width, height;

	IMB_stereo_dimensions(im_format->stereo_output.display_mode, x, y, &width, &height);
	r_rectf = MEM_mallocN(channels * sizeof(float) * width * height, __func__);

	imb_stereo_data_initialize(&s3d_data, im_format, x, y, channels, NULL, NULL, NULL, left, right, r_rectf);
	imb_stereo_doit(&s3d_data, &im_format->stereo_output);

	return r_rectf;
}

/* left/right are always float */
ImBuf *IMB_stereoImBuf(ImageFormatData *im_format, ImBuf *left, ImBuf *right)
{
	ImBuf *r_ibuf = NULL;
	Stereo3DData s3d_data = {{NULL}};
	size_t width, height;
	const bool is_float = im_format->depth > 8;

	IMB_stereo_dimensions(im_format->stereo_output.display_mode, left->x, left->y, &width, &height);
	r_ibuf = IMB_allocImBuf(width, height, left->planes, (is_float ? IB_rectfloat : IB_rect));

	imb_stereo_data_initialize(&s3d_data, im_format, left->x, left->y, 4,
		                         (int *)left->rect, (int *)right->rect, (int *)r_ibuf->rect,
		                         left->rect_float, right->rect_float, r_ibuf->rect_float);

	imb_stereo_doit(&s3d_data, &im_format->stereo_output);
	return r_ibuf;
}

static void imb_stereo_doit(Stereo3DData *s3d_data, StereoDisplay *s3d)
{
	switch (s3d->display_mode) {
		case S3D_DISPLAY_ANAGLYPH:
			imb_stereo_anaglyph(s3d_data, s3d->anaglyph_type);
			break;
		case S3D_DISPLAY_BLURAY:
			break;
		case S3D_DISPLAY_INTERLACE:
			imb_stereo_interlace(s3d_data, s3d->interlace_type, (s3d->flag & S3D_INTERLACE_SWAP));
			break;
		case S3D_DISPLAY_SIDEBYSIDE:
			imb_stereo_sidebyside(s3d_data, (s3d->flag & S3D_SIDEBYSIDE_CROSSEYED));
			break;
		case S3D_DISPLAY_TOPBOTTOM:
			imb_stereo_topbottom(s3d_data);
			break;
		default:
			break;
	}
}
