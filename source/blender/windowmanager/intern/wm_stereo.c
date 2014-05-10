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

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "BIF_gl.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_report.h"

#include "GHOST_C-api.h"

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

	for (view=0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);

		if (view == STEREO_LEFT_ID)
			glDrawBuffer(GL_BACK_LEFT);
		else //STEREO_RIGHT_ID
			glDrawBuffer(GL_BACK_RIGHT);

		wm_triple_draw_textures(win, drawdata->triple, 1.0);
	}
}

static void wm_method_draw_stereo_epilepsy(wmWindow *win)
{
	wmDrawData *drawdata;
	static bool view = 0;
	static double start = 0;

	if( (PIL_check_seconds_timer() - start) >= U.stereo_epilepsy_interval) {
		start = PIL_check_seconds_timer();
		view = !view;
	}

	drawdata = BLI_findlink(&win->drawdata, view * 2 + 1);

	wm_triple_draw_textures(win, drawdata->triple, 1.0);
}

static GLuint left_interlace_mask[32];
static GLuint right_interlace_mask[32];
static int interlace_prev_type = -1;
static char interlace_prev_swap = -1;

static void wm_interlace_create_masks(void)
{
	GLuint pattern;
	char i;
	bool swap = (U.stereo_flag & S3D_INTERLACE_SWAP);

	if (interlace_prev_type == U.stereo_interlace_type && interlace_prev_swap == swap)
		return;

	switch(U.stereo_interlace_type) {
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
	interlace_prev_type = U.stereo_interlace_type;
	interlace_prev_swap = swap;
}

static void wm_method_draw_stereo_interlace(wmWindow *win)
{
	wmDrawData *drawdata;
	int view;

	wm_interlace_create_masks();

	for (view=0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);

		glEnable(GL_POLYGON_STIPPLE);
		glPolygonStipple(view ? right_interlace_mask : left_interlace_mask);

		wm_triple_draw_textures(win, drawdata->triple, 1.0);
		glDisable(GL_POLYGON_STIPPLE);
	}
}

static void wm_method_draw_stereo_anaglyph(wmWindow *win)
{
	wmDrawData *drawdata;
	int view, bit;

	for (view=0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);

		bit = view + 1;
		switch(U.stereo_anaglyph_type) {
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

		wm_triple_draw_textures(win, drawdata->triple, 1.0);

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
	bool cross_eyed = (U.stereo_flag & S3D_SIDEBYSIDE_CROSSEYED);

	for (view=0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);
		triple = drawdata->triple;

		soffx = WM_window_pixels_x(win) * 0.5;
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
				glVertex2f(soffx + (offx * 0.5), offy);

				glTexCoord2f(ratiox + halfx, halfy);
				glVertex2f(soffx + ((offx + sizex) * 0.5), offy);

				glTexCoord2f(ratiox + halfx, ratioy + halfy);
				glVertex2f(soffx + ((offx + sizex) * 0.5), offy + sizey);

				glTexCoord2f(halfx, ratioy + halfy);
				glVertex2f(soffx + (offx * 0.5), offy + sizey);
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

	for (view=0; view < 2; view ++) {
		drawdata = BLI_findlink(&win->drawdata, (view * 2) + 1);
		triple = drawdata->triple;

		if (view == STEREO_LEFT_ID) {
			soffy = WM_window_pixels_y(win) * 0.5;
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
				glVertex2f(offx, soffy + (offy * 0.5));

				glTexCoord2f(ratiox + halfx, halfy);
				glVertex2f(offx + sizex, soffy + (offy * 0.5));

				glTexCoord2f(ratiox + halfx, ratioy + halfy);
				glVertex2f(offx + sizex, soffy + ((offy + sizey) * 0.5));

				glTexCoord2f(halfx, ratioy + halfy);
				glVertex2f(offx, soffy + ((offy + sizey) * 0.5));
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
	switch (U.stereo_display)
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
	return ELEM3(stereo_display,
	             S3D_DISPLAY_SIDEBYSIDE,
	             S3D_DISPLAY_TOPBOTTOM,
	             S3D_DISPLAY_PAGEFLIP);
}

static bool wm_stereo_required(bScreen *screen)
{
	/* where there is no */
	ScrArea *sa;
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		SpaceLink *sl;
		for (sl = sa->spacedata.first; sl; sl= sl->next) {

			if (sl->spacetype == SPACE_VIEW3D) {
				View3D *v3d = (View3D*) sl;
				if (v3d->stereo_camera == STEREO_3D_ID)
					return true;
			}
			if (sl->spacetype == SPACE_IMAGE) {
				SpaceImage *sima = (SpaceImage *) sl;
// XXX MV - waiting to image editor refactor, basically we will know if the image is 3d AND that it has 3d selected
//					if (sima->iuser.view == STEREO_3D_ID)
					return true;
			}
		}
	}

	return false;
}

bool WM_stereo_enabled(wmWindow *win, bool only_fullscreen_test)
{
	bScreen *screen = win->screen;

	if ((only_fullscreen_test == false) && wm_stereo_required(screen) == false)
		return false;

	if (wm_stereo_need_fullscreen(U.stereo_display))
		return (GHOST_GetWindowState(win->ghostwin) == GHOST_kWindowStateFullScreen);

	return true;
}

int wm_stereo_toggle_exec(bContext *C, wmOperator *op)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	GHOST_TWindowState state;

	if (G.background)
		return OPERATOR_CANCELLED;

	/* FullScreen or Normal */
	state = GHOST_GetWindowState(win->ghostwin);

	/* pagelfip requires a new window to be created with the proper OS flags */
	if (U.stereo_display == S3D_DISPLAY_PAGEFLIP) {
		if (wm_window_duplicate_exec(C, op) == OPERATOR_FINISHED) {
			wm_window_close(C, wm, win);
			win = (wmWindow *)wm->windows.last;
		}
		else {
			BKE_reportf(op->reports, RPT_ERROR, "Fail to create a window compatible with time sequential (page-flip) display method");
			return OPERATOR_CANCELLED;
		}
	}

	if (wm_stereo_need_fullscreen(U.stereo_display)) {
		if (state != GHOST_kWindowStateFullScreen)
			GHOST_SetWindowState(win->ghostwin, GHOST_kWindowStateFullScreen);
	}

	WM_event_add_notifier(C, NC_WINDOW, NULL);
	return OPERATOR_FINISHED;
}

