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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_stereo.c
 *  \ingroup wm
 */


#include <stdlib.h>
#include <string.h>
#include <GL/glew.h>

#include "DNA_listBase.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "RNA_access.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "BIF_gl.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_report.h"

#include "GHOST_C-api.h"

#include "ED_node.h"
#include "ED_view3d.h"
#include "ED_screen.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"

#include "PIL_time.h"

#include "RE_engine.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_stereo.h"
#include "wm_draw.h" /* wmDrawTriple */
#include "wm_window.h"
#include "wm_event_system.h"

static void wm_method_draw_stereo_pageflip(wmWindow *win)
{
	wmDrawData *drawdata;
	int view;

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);

		if (view == STEREO_LEFT_ID)
			glDrawBuffer(GL_BACK_LEFT);
		else //STEREO_RIGHT_ID
			glDrawBuffer(GL_BACK_RIGHT);

		wm_triple_draw_textures(win, drawdata->triple, 1.0f);
	}
}

static void wm_method_draw_stereo_epilepsy(wmWindow *win)
{
	wmDrawData *drawdata;
	static bool view = false;
	static double start = 0.0;

	if( (PIL_check_seconds_timer() - start) >= win->stereo_display.epilepsy_interval) {
		start = PIL_check_seconds_timer();
		view = !view;
	}

	drawdata = BLI_findlink(&win->drawdata, view * 2 + 1);

	wm_triple_draw_textures(win, drawdata->triple, 1.0f);
}

static GLuint left_interlace_mask[32];
static GLuint right_interlace_mask[32];
static enum eStereoInterlaceType interlace_prev_type = -1;
static char interlace_prev_swap = -1;

static void wm_interlace_create_masks(wmWindow *win)
{
	GLuint pattern;
	char i;
	bool swap = (win->stereo_display.flag & S3D_INTERLACE_SWAP);
	enum eStereoInterlaceType interlace_type = win->stereo_display.interlace_type;

	if (interlace_prev_type == interlace_type && interlace_prev_swap == swap)
		return;

	switch(interlace_type) {
		case S3D_INTERLACE_ROW:
			pattern = 0x00000000;
			pattern = swap? ~pattern : pattern;
			for(i = 0; i < 32; i += 2) {
				left_interlace_mask[i] = pattern;
				right_interlace_mask[i] = ~pattern;
			}
			for(i = 1; i < 32; i += 2) {
				left_interlace_mask[i] = ~pattern;
				right_interlace_mask[i] = pattern;
			}
			break;
		case S3D_INTERLACE_COLUMN:
			pattern = 0x55555555;
			pattern = swap? ~pattern : pattern;
			for(i = 0; i < 32; i++) {
				left_interlace_mask[i] = pattern;
				right_interlace_mask[i] = ~pattern;
			}
			break;
		case S3D_INTERLACE_CHECKERBOARD:
		default:
			pattern = 0x55555555;
			pattern = swap? ~pattern : pattern;
			for(i = 0; i < 32; i += 2) {
				left_interlace_mask[i] = pattern;
				right_interlace_mask[i] = ~pattern;
			}
			for(i = 1; i < 32; i += 2) {
				left_interlace_mask[i] = ~pattern;
				right_interlace_mask[i] = pattern;
			}
			break;
	}
	interlace_prev_type = interlace_type;
	interlace_prev_swap = swap;
}

static void wm_method_draw_stereo_interlace(wmWindow *win)
{
	wmDrawData *drawdata;
	int view;

	wm_interlace_create_masks(win);

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);

		glEnable(GL_POLYGON_STIPPLE);
		glPolygonStipple(view ? (GLubyte *) right_interlace_mask : (GLubyte *) left_interlace_mask);

		wm_triple_draw_textures(win, drawdata->triple, 1.0f);
		glDisable(GL_POLYGON_STIPPLE);
	}
}

static void wm_method_draw_stereo_anaglyph(wmWindow *win)
{
	wmDrawData *drawdata;
	int view, bit;

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);

		bit = view + 1;
		switch(win->stereo_display.anaglyph_type) {
			case S3D_ANAGLYPH_REDCYAN:
				glColorMask(1&bit, 2&bit, 2&bit, false);
				break;
			case S3D_ANAGLYPH_GREENMAGENTA:
				glColorMask(2&bit, 1&bit, 2&bit, false);
				break;
			case S3D_ANAGLYPH_YELLOWBLUE:
				glColorMask(1&bit, 1&bit, 2&bit, false);
				break;
		}

		wm_triple_draw_textures(win, drawdata->triple, 1.0f);

		glColorMask(true, true, true, true);
	}
}

static void wm_method_draw_stereo_sidebyside(wmWindow *win)
{
	wmDrawData *drawdata;
	wmDrawTriple *triple;
	float halfx, halfy, ratiox, ratioy;
	int x, y, sizex, sizey, offx, offy;
	float alpha = 1.0f;
	int view;
	int soffx;
	bool cross_eyed = (win->stereo_display.flag & S3D_SIDEBYSIDE_CROSSEYED);

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);
		triple = drawdata->triple;

		soffx = WM_window_pixels_x(win) * 0.5f;
		if (view == STEREO_LEFT_ID) {
			if(!cross_eyed)
				soffx = 0;
		}
		else { //RIGHT_LEFT_ID
			if(cross_eyed)
				soffx = 0;
		}

		glEnable(triple->target);

		for (y = 0, offy = 0; y < triple->ny; offy += triple->y[y], y++) {
			for (x = 0, offx = 0; x < triple->nx; offx += triple->x[x], x++) {
				sizex = (x == triple->nx - 1) ? WM_window_pixels_x(win) - offx : triple->x[x];
				sizey = (y == triple->ny - 1) ? WM_window_pixels_y(win) - offy : triple->y[y];

				/* wmOrtho for the screen has this same offset */
				ratiox = sizex;
				ratioy = sizey;
				halfx = GLA_PIXEL_OFS;
				halfy = GLA_PIXEL_OFS;

				/* texture rectangle has unnormalized coordinates */
				if (triple->target == GL_TEXTURE_2D) {
					ratiox /= triple->x[x];
					ratioy /= triple->y[y];
					halfx /= triple->x[x];
					halfy /= triple->y[y];
				}

				glBindTexture(triple->target, triple->bind[x + y * triple->nx]);

				glColor4f(1.0f, 1.0f, 1.0f, alpha);
				glBegin(GL_QUADS);
				glTexCoord2f(halfx, halfy);
				glVertex2f(soffx + (offx * 0.5f), offy);

				glTexCoord2f(ratiox + halfx, halfy);
				glVertex2f(soffx + ((offx + sizex) * 0.5f), offy);

				glTexCoord2f(ratiox + halfx, ratioy + halfy);
				glVertex2f(soffx + ((offx + sizex) * 0.5f), offy + sizey);

				glTexCoord2f(halfx, ratioy + halfy);
				glVertex2f(soffx + (offx * 0.5f), offy + sizey);
				glEnd();
			}
		}

		glBindTexture(triple->target, 0);
		glDisable(triple->target);
		glColorMask(true, true, true, true);
	}
}

static void wm_method_draw_stereo_topbottom(wmWindow *win)
{
	wmDrawData *drawdata;
	wmDrawTriple *triple;
	float halfx, halfy, ratiox, ratioy;
	int x, y, sizex, sizey, offx, offy;
	float alpha = 1.0f;
	int view;
	int soffy;

	for (view = 0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);
		triple = drawdata->triple;

		if (view == STEREO_LEFT_ID) {
			soffy = WM_window_pixels_y(win) * 0.5f;
		}
		else { //STEREO_RIGHT_ID
			soffy = 0;
		}

		glEnable(triple->target);

		for (y = 0, offy = 0; y < triple->ny; offy += triple->y[y], y++) {
			for (x = 0, offx = 0; x < triple->nx; offx += triple->x[x], x++) {
				sizex = (x == triple->nx - 1) ? WM_window_pixels_x(win) - offx : triple->x[x];
				sizey = (y == triple->ny - 1) ? WM_window_pixels_y(win) - offy : triple->y[y];

				/* wmOrtho for the screen has this same offset */
				ratiox = sizex;
				ratioy = sizey;
				halfx = GLA_PIXEL_OFS;
				halfy = GLA_PIXEL_OFS;

				/* texture rectangle has unnormalized coordinates */
				if (triple->target == GL_TEXTURE_2D) {
					ratiox /= triple->x[x];
					ratioy /= triple->y[y];
					halfx /= triple->x[x];
					halfy /= triple->y[y];
				}

				glBindTexture(triple->target, triple->bind[x + y * triple->nx]);

				glColor4f(1.0f, 1.0f, 1.0f, alpha);
				glBegin(GL_QUADS);
				glTexCoord2f(halfx, halfy);
				glVertex2f(offx, soffy + (offy * 0.5f));

				glTexCoord2f(ratiox + halfx, halfy);
				glVertex2f(offx + sizex, soffy + (offy * 0.5f));

				glTexCoord2f(ratiox + halfx, ratioy + halfy);
				glVertex2f(offx + sizex, soffy + ((offy + sizey) * 0.5f));

				glTexCoord2f(halfx, ratioy + halfy);
				glVertex2f(offx, soffy + ((offy + sizey) * 0.5f));
				glEnd();
			}
		}

		glBindTexture(triple->target, 0);
		glDisable(triple->target);
		glColorMask(true, true, true, true);
	}
}

void wm_method_draw_stereo(bContext *UNUSED(C), wmWindow *win)
{
	switch (win->stereo_display.display_mode)
	{
		case S3D_DISPLAY_ANAGLYPH:
			wm_method_draw_stereo_anaglyph(win);
			break;
		case S3D_DISPLAY_EPILEPSY:
			wm_method_draw_stereo_epilepsy(win);
			break;
		case S3D_DISPLAY_INTERLACE:
			wm_method_draw_stereo_interlace(win);
			break;
		case S3D_DISPLAY_PAGEFLIP:
			wm_method_draw_stereo_pageflip(win);
			break;
		case S3D_DISPLAY_SIDEBYSIDE:
			wm_method_draw_stereo_sidebyside(win);
			break;
		case S3D_DISPLAY_TOPBOTTOM:
			wm_method_draw_stereo_topbottom(win);
			break;
		default:
			break;
	}
}

static bool wm_stereo_need_fullscreen(eStereoDisplayMode stereo_display)
{
	return ELEM(stereo_display,
	            S3D_DISPLAY_SIDEBYSIDE,
	            S3D_DISPLAY_TOPBOTTOM,
	            S3D_DISPLAY_PAGEFLIP);
}

/*
 * return true if any active area requires to see in 3D
 */
static bool wm_stereo_required(bContext *C, bScreen *screen)
{
	ScrArea *sa;
	View3D *v3d;
	SpaceImage *sima;
	SpaceNode *snode;
	Scene *sce = CTX_data_scene(C);
	const bool is_multiview = (sce->r.scemode & R_MULTIVIEW);

	for (sa = screen->areabase.first; sa; sa = sa->next) {
		switch (sa->spacetype) {
			case SPACE_VIEW3D:
			{
				if (!is_multiview)
					continue;

				v3d = (View3D *)sa->spacedata.first;
				if (v3d->camera && v3d->stereo_camera == STEREO_3D_ID) {
					ARegion *ar;
					for (ar = sa->regionbase.first; ar; ar = ar->next) {
						if (ar->regiondata && ar->regiontype == RGN_TYPE_WINDOW) {
							RegionView3D *rv3d = ar->regiondata;
							if (rv3d->persp == RV3D_CAMOB) {
								return true;
							}
						}
					}
				}
				break;
			}
			case SPACE_IMAGE:
			{
				/* images should always show in stereo, even if
				 * the file doesn't have views enabled */
				sima = (SpaceImage *) sa->spacedata.first;
				if ((sima->image) && (sima->image->flag & IMA_IS_STEREO) &&
					(sima->iuser.flag & IMA_SHOW_STEREO))
				{
					return true;
				}
				break;
			}
			case SPACE_NODE:
			{
				if (!is_multiview)
					continue;

				snode = (SpaceNode *) sa->spacedata.first;
				if ((snode->flag & SNODE_BACKDRAW) && ED_node_is_compositor(snode)) {
					return true;
				}
			}
		}
	}

	return false;
}

bool WM_stereo_enabled(bContext *C, wmWindow *win, bool only_fullscreen_test)
{
	bScreen *screen = win->screen;

	if ((only_fullscreen_test == false) && (wm_stereo_required(C, screen) == false))
		return false;

	if (wm_stereo_need_fullscreen(win->stereo_display.display_mode))
		return (GHOST_GetWindowState(win->ghostwin) == GHOST_kWindowStateFullScreen);

	return true;
}

static void wm_stereo_set_properties(bContext *C, wmOperator *op)
{
	wmWindow *win = CTX_wm_window(C);
	StereoDisplay *s3d = &win->stereo_display;

	s3d->display_mode = RNA_enum_get(op->ptr, "display_mode");
	s3d->anaglyph_type = RNA_enum_get(op->ptr, "anaglyph_type");
	s3d->interlace_type = RNA_enum_get(op->ptr, "interlace_type");
	s3d->epilepsy_interval = RNA_float_get(op->ptr, "epilepsy_interval");

	s3d->flag = 0;

	if (RNA_boolean_get(op->ptr, "use_interlace_swap"))
		s3d->flag |= S3D_INTERLACE_SWAP;

	if (RNA_boolean_get(op->ptr, "use_sidebyside_crosseyed"))
		s3d->flag |= S3D_SIDEBYSIDE_CROSSEYED;
}

int wm_stereo_toggle_exec(bContext *C, wmOperator *op)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	GHOST_TWindowState state;

	if (G.background)
		return OPERATOR_CANCELLED;

	wm_stereo_set_properties(C, op);

	/* FullScreen or Normal */
	state = GHOST_GetWindowState(win->ghostwin);

	/* pagelfip requires a new window to be created with the proper OS flags */
	if (win->stereo_display.display_mode == S3D_DISPLAY_PAGEFLIP) {
		if (wm_window_duplicate_exec(C, op) == OPERATOR_FINISHED) {
			wm_window_close(C, wm, win);
			win = (wmWindow *)wm->windows.last;
		}
		else {
			BKE_reportf(op->reports, RPT_ERROR, "Fail to create a window compatible with time sequential (page-flip) display method");
			return OPERATOR_CANCELLED;
		}
	}

	if (wm_stereo_need_fullscreen(win->stereo_display.display_mode)) {
		if (state != GHOST_kWindowStateFullScreen)
			GHOST_SetWindowState(win->ghostwin, GHOST_kWindowStateFullScreen);
	}

	WM_event_add_notifier(C, NC_WINDOW, NULL);
	return OPERATOR_FINISHED;
}

int wm_stereo_toggle_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	wmWindow *win = CTX_wm_window(C);
	StereoDisplay *s3d = &win->stereo_display;
	PropertyRNA *prop;

	prop = RNA_struct_find_property(op->ptr, "display_mode");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, s3d->display_mode);
	}

	prop = RNA_struct_find_property(op->ptr, "anaglyph_type");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, s3d->anaglyph_type);
	}

	prop = RNA_struct_find_property(op->ptr, "interlace_type");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, s3d->interlace_type);
	}

	prop = RNA_struct_find_property(op->ptr, "epilepsy_interval");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set(op->ptr, prop, s3d->epilepsy_interval);
	}

	prop = RNA_struct_find_property(op->ptr, "use_interlace_swap");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (s3d->flag & S3D_INTERLACE_SWAP));
	}

	prop = RNA_struct_find_property(op->ptr, "use_sidebyside_crosseyed");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (s3d->flag & S3D_SIDEBYSIDE_CROSSEYED));
	}

	return wm_stereo_toggle_exec(C, op);
}

