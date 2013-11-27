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
 * Contributor(s): Dalai Felinto, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_fps.c
 *  \ingroup spview3d
 */

/* defines VIEW3D_OT_fps modal operator */

//#define NDOF_FPS_DEBUG
//#define NDOF_FPS_DRAW_TOOMUCH  /* is this needed for ndof? - commented so redraw doesnt thrash - campbell */
#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "BKE_depsgraph.h" /* for fps mode updating */

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_transform.h"

#include "PIL_time.h" /* smoothview */

#include "view3d_intern.h"  /* own include */

#define FPS_GRAVITY 9.80668 /* m/s2 */

/* prototypes */
static float getVelocityZeroTime(float velocity);

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
enum {
	FPS_MODAL_CANCEL = 1,
	FPS_MODAL_CONFIRM,
	FPS_MODAL_DIR_FORWARD,
	FPS_MODAL_DIR_FORWARD_STOP,
	FPS_MODAL_DIR_BACKWARD,
	FPS_MODAL_DIR_BACKWARD_STOP,
	FPS_MODAL_DIR_LEFT,
	FPS_MODAL_DIR_LEFT_STOP,
	FPS_MODAL_DIR_RIGHT,
	FPS_MODAL_DIR_RIGHT_STOP,
	FPS_MODAL_DIR_UP,
	FPS_MODAL_DIR_UP_STOP,
	FPS_MODAL_DIR_DOWN,
	FPS_MODAL_DIR_DOWN_STOP,
	FPS_MODAL_AXIS_LOCK_X,
	FPS_MODAL_AXIS_LOCK_Z,
	FPS_MODAL_PRECISION_ENABLE,
	FPS_MODAL_PRECISION_DISABLE,
	FPS_MODAL_FREELOOK_ENABLE,
	FPS_MODAL_FREELOOK_DISABLE,
	FPS_MODAL_SPEED,
	FPS_MODAL_FAST_ENABLE,
	FPS_MODAL_FAST_DISABLE,
	FPS_MODAL_SLOW_ENABLE,
	FPS_MODAL_SLOW_DISABLE,
	FPS_MODAL_JUMP,
	FPS_MODAL_JUMP_STOP,
	FPS_MODAL_TELEPORT,
	FPS_MODAL_WALK,
	FPS_MODAL_FLY,
	FPS_MODAL_TOGGLE,
};

enum {
	FPS_BIT_FORWARD  = 1 << 0,
	FPS_BIT_BACKWARD = 1 << 1,
	FPS_BIT_LEFT     = 1 << 2,
	FPS_BIT_RIGHT    = 1 << 3,
	FPS_BIT_UP       = 1 << 4,
	FPS_BIT_DOWN     = 1 << 5,
};

/* relative view axis locking - xlock, zlock */
typedef enum eFPSPanState {
	/* disabled */
	FPS_AXISLOCK_STATE_OFF    = 0,

	/* enabled but not checking because mouse hasn't moved outside the margin since locking was checked an not needed
	 * when the mouse moves, locking is set to 2 so checks are done. */
	FPS_AXISLOCK_STATE_IDLE   = 1,

	/* mouse moved and checking needed, if no view altering is done its changed back to #FPS_AXISLOCK_STATE_IDLE */
	FPS_AXISLOCK_STATE_ACTIVE = 2
} eFPSPanState;

typedef enum eFPSTeleportState {
	FPS_TELEPORT_STATE_OFF = 0,
	FPS_TELEPORT_STATE_ON,
} eFPSTeleportState;

typedef enum eFPSGravityState {
	FPS_GRAVITY_STATE_OFF = 0,
	FPS_GRAVITY_STATE_JUMP,
	FPS_GRAVITY_STATE_START,
	FPS_GRAVITY_STATE_ON,
} eFPSGravityState;

/* called in transform_ops.c, on each regeneration of keymaps  */
void fps_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{FPS_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{FPS_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

		{FPS_MODAL_DIR_FORWARD, "FORWARD", 0, "Move Forward", ""},
		{FPS_MODAL_DIR_BACKWARD, "BACKWARD", 0, "Move Backward", ""},
		{FPS_MODAL_DIR_LEFT, "LEFT", 0, "Move Left (Strafe)", ""},
		{FPS_MODAL_DIR_RIGHT, "RIGHT", 0, "Move Right (Strafe)", ""},
		{FPS_MODAL_DIR_UP, "UP", 0, "Move Up", ""},
		{FPS_MODAL_DIR_DOWN, "DOWN", 0, "Move Down", ""},

		{FPS_MODAL_DIR_FORWARD_STOP, "FORWARD_STOP", 0, "Stop Move Forward", ""},
		{FPS_MODAL_DIR_BACKWARD_STOP, "BACKWARD_STOP", 0, "Stop Mode Backward", ""},
		{FPS_MODAL_DIR_LEFT_STOP, "LEFT_STOP", 0, "Stop Move Left", ""},
		{FPS_MODAL_DIR_RIGHT_STOP, "RIGHT_STOP", 0, "Stop Mode Right", ""},
		{FPS_MODAL_DIR_UP_STOP, "UP_STOP", 0, "Stop Move Up", ""},
		{FPS_MODAL_DIR_DOWN_STOP, "DOWN_STOP", 0, "Stop Mode Down", ""},

		{FPS_MODAL_AXIS_LOCK_X, "AXIS_LOCK_X", 0, "X Axis Correction", "X axis correction (toggle)"},
		{FPS_MODAL_AXIS_LOCK_Z, "AXIS_LOCK_Z", 0, "X Axis Correction", "Z axis correction (toggle)"},

		{FPS_MODAL_TELEPORT, "TELEPORT", 0, "Teleport", "Move forward a few units at once"},

		{FPS_MODAL_FAST_ENABLE, "FAST_ENABLE", 0, "Fast Enable", "Move faster (walk or fly)"},
		{FPS_MODAL_FAST_DISABLE, "FAST_DISABLE", 0, "Fast Disable", "Resume regular speed"},

		{FPS_MODAL_SLOW_ENABLE, "SLOW_ENABLE", 0, "Slow Enable", "Move slower (walk or fly)"},
		{FPS_MODAL_SLOW_DISABLE, "SLOW_DISABLE", 0, "Slow Disable", "Resume regular speed"},

		{FPS_MODAL_JUMP, "JUMP", 0, "Jump", "Jump when in walk mode"},
		{FPS_MODAL_JUMP_STOP, "JUMP_STOP", 0, "Jump Stop", "Stop pushing jump forward"},

		{FPS_MODAL_WALK, "WALK", 0, "Walk", "Move around following the floor"},
		{FPS_MODAL_FLY, "FLY", 0, "Fly", "Move around in the direction the camera is facing"},

		{0, NULL, 0, NULL, NULL}};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D FPS Modal");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items)
		return;

	keymap = WM_modalkeymap_add(keyconf, "View3D FPS Modal", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_ANY, KM_ANY, 0, FPS_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_ANY, KM_ANY, 0, FPS_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, FPS_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_FAST_ENABLE);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_FAST_DISABLE);

	WM_modalkeymap_add_item(keymap, LEFTALTKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_SLOW_ENABLE);
	WM_modalkeymap_add_item(keymap, LEFTALTKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_SLOW_DISABLE);

	/* WASD */
	WM_modalkeymap_add_item(keymap, WKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_DIR_FORWARD);
	WM_modalkeymap_add_item(keymap, SKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_DIR_BACKWARD);
	WM_modalkeymap_add_item(keymap, AKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_DIR_LEFT);
	WM_modalkeymap_add_item(keymap, DKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_DIR_RIGHT);
	WM_modalkeymap_add_item(keymap, EKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_DIR_UP);
	WM_modalkeymap_add_item(keymap, QKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_DIR_DOWN);

	WM_modalkeymap_add_item(keymap, WKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_FORWARD_STOP);
	WM_modalkeymap_add_item(keymap, SKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_BACKWARD_STOP);
	WM_modalkeymap_add_item(keymap, AKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_LEFT_STOP);
	WM_modalkeymap_add_item(keymap, DKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_RIGHT_STOP);
	WM_modalkeymap_add_item(keymap, EKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_UP_STOP);
	WM_modalkeymap_add_item(keymap, QKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_DOWN_STOP);

	WM_modalkeymap_add_item(keymap, UPARROWKEY, KM_PRESS, 0, 0, FPS_MODAL_DIR_FORWARD);
	WM_modalkeymap_add_item(keymap, DOWNARROWKEY, KM_PRESS, 0, 0, FPS_MODAL_DIR_BACKWARD);
	WM_modalkeymap_add_item(keymap, LEFTARROWKEY, KM_PRESS, 0, 0, FPS_MODAL_DIR_LEFT);
	WM_modalkeymap_add_item(keymap, RIGHTARROWKEY, KM_PRESS, 0, 0, FPS_MODAL_DIR_RIGHT);

	WM_modalkeymap_add_item(keymap, UPARROWKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_FORWARD_STOP);
	WM_modalkeymap_add_item(keymap, DOWNARROWKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_BACKWARD_STOP);
	WM_modalkeymap_add_item(keymap, LEFTARROWKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_LEFT_STOP);
	WM_modalkeymap_add_item(keymap, RIGHTARROWKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_DIR_RIGHT_STOP);

	WM_modalkeymap_add_item(keymap, XKEY, KM_PRESS, 0, 0, FPS_MODAL_AXIS_LOCK_X);
	WM_modalkeymap_add_item(keymap, ZKEY, KM_PRESS, 0, 0, FPS_MODAL_AXIS_LOCK_Z);

	WM_modalkeymap_add_item(keymap, RKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_WALK);
	WM_modalkeymap_add_item(keymap, FKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_FLY);

	WM_modalkeymap_add_item(keymap, SPACEKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_JUMP);
	WM_modalkeymap_add_item(keymap, SPACEKEY, KM_RELEASE, KM_ANY, 0, FPS_MODAL_JUMP_STOP);

	WM_modalkeymap_add_item(keymap, TKEY, KM_PRESS, KM_ANY, 0, FPS_MODAL_TELEPORT);

	WM_modalkeymap_add_item(keymap, TABKEY, KM_PRESS, 0, 0, FPS_MODAL_TOGGLE);

	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_fps");
}


typedef struct FPSTeleport
{
	eFPSTeleportState state;
	float duration; /* from user preferences */
	float origin[3];
	float direction[3];
	double initial_time;
	eViewNavigation_Method navigation_mode; /* teleport always set FLY mode on */

} FPSTeleport;

typedef struct FPSInfo {
	/* context stuff */
	RegionView3D *rv3d;
	View3D *v3d;
	ARegion *ar;
	Scene *scene;

	wmTimer *timer; /* needed for redraws */

	short state;
	bool redraw;

	int mval[2]; /* latest 2D mouse values */
	int prev_mval[2]; /* previous mouse values */
	wmNDOFMotionData *ndof;  /* latest 3D mouse values */

	/* fps state state */
	float speed; /* the speed the view is moving per redraw */
	bool pan_view; /* when true, pan the view instead of rotating */

	eFPSPanState xlock, zlock;
	float xlock_momentum, zlock_momentum; /* nicer dynamics */

	/* root most parent */
	Object *root_parent;

	/* backup values */
	float dist_backup; /* backup the views distance since we use a zero dist for fps mode */
	float ofs_backup[3]; /* backup the views offset in case the user cancels navigation in non camera mode */

	/* backup the views quat in case the user cancels fpsing in non camera mode.
	 * (quat for view, eul for camera) */
	float rot_backup[4];
	short persp_backup; /* remember if were ortho or not, only used for restoring the view if it was a ortho view */

	/* are we fpsing an ortho camera in perspective view,
	 * which was originall in ortho view?
	 * could probably figure it out but better be explicit */
	bool is_ortho_cam;
	void *obtfm; /* backup the objects transform */

	/* compare between last state */
	double time_lastwheel; /* used to accelerate when using the mousewheel a lot */
	double time_lastdraw; /* time between draws */

	void *draw_handle_pixel;

	/* use for some lag */
	float dvec_prev[3]; /* old for some lag */

	/* for parenting calculation */
	float view_mat_prev[4][4];

	/* walk/fly */
	short navigation_mode;

	/* teleport */
	FPSTeleport teleport;

	/* look speed factor - user preferences */
	float mouse_sensitivity;

	/* speed adjustments */
	bool is_fast;
	bool is_slow;

	/* gravity system */
	eFPSGravityState gravity;

	/* height to use in walk mode */
	float camera_height;

	/* counting system to allow movement to continue if a direction (WASD) key is still pressed */
	int active_directions;

	float speed_jump;
	float jump_height; /* maximum jump height */
	float speed_boost; /* to use for fast/slow speeds */

} FPSInfo;

static void drawFPSPixel(const struct bContext *UNUSED(C), ARegion *UNUSED(ar), void *arg)
{
	FPSInfo *fps = arg;
	/* draws an aim/cross in the center */

	const int outter_length = 20;
	const int inner_length = 4;
	const int xoff = fps->ar->winx / 2;
	const int yoff = fps->ar->winy / 2;

	cpack(0);

	glBegin(GL_LINES);
	/* North */
	glVertex2i(xoff, yoff + inner_length);
	glVertex2i(xoff, yoff + outter_length);

	/* East */
	glVertex2i(xoff + inner_length, yoff);
	glVertex2i(xoff + outter_length, yoff);

	/* South */
	glVertex2i(xoff, yoff - inner_length);
	glVertex2i(xoff, yoff - outter_length);

	/* West */
	glVertex2i(xoff - inner_length, yoff);
	glVertex2i(xoff - outter_length, yoff);
	glEnd();

	/* XXX print shortcuts for modal options, and current values */
}

static void SetNavigationMode(FPSInfo *fps, const eViewNavigation_Method navigation_mode)
{
	if (navigation_mode == FPS_NAVIGATION_FLY) {
		fps->navigation_mode = FPS_NAVIGATION_FLY;
		fps->gravity = FPS_GRAVITY_STATE_OFF;
	}
	else { /* FPS_NAVIGATION_WALK */
		fps->navigation_mode = FPS_NAVIGATION_WALK;
		fps->gravity = FPS_GRAVITY_STATE_START;
	}
}

static bool getFloorDistance(bContext *C, RegionView3D *rv3d, float dvec[3], float *ray_distance)
{
	float dummy_dist_px = 0;
	float ray_normal[3] = {0, 0, -1}; /* down */
	float ray_start[3];
	float r_location[3];
	float r_normal[3];
	bool ret;

	*ray_distance = TRANSFORM_DIST_MAX_RAY;

	copy_v3_v3(ray_start, rv3d->viewinv[3]);

	if (dvec) {
		add_v3_v3(ray_start, dvec);
	}

	ret = snapObjectsRayEx(CTX_data_scene(C), NULL, NULL, NULL, NULL, SCE_SNAP_MODE_FACE,
	                       NULL, NULL,
	                       ray_start, ray_normal, ray_distance,
	                       NULL, &dummy_dist_px, r_location, r_normal, SNAP_ALL);

	return ret;
}

/**
 ray_distance = distance to the hit point
 r_location = location of the hit point
 r_normal = normal of the hit surface
 */
static bool getTeleportRay(bContext *C, RegionView3D *rv3d, float r_location[3], float r_normal[3], float *ray_distance)
{
	float dummy_dist_px = 0;
	float ray_normal[3] = {0, 0, 1}; /* forward */
	float ray_start[3];
	float mat[3][3]; /* 3x3 copy of the view matrix so we can move along the view axis */
	bool ret;

	*ray_distance = TRANSFORM_DIST_MAX_RAY;

	copy_v3_v3(ray_start, rv3d->viewinv[3]);
	copy_m3_m4(mat, rv3d->viewinv);

	mul_m3_v3(mat, ray_normal);

	mul_v3_fl(ray_normal, -1);
	normalize_v3(ray_normal);

	ret = snapObjectsRayEx(CTX_data_scene(C), NULL, NULL, NULL, NULL, SCE_SNAP_MODE_FACE,
	                       NULL, NULL,
	                       ray_start, ray_normal, ray_distance,
	                       NULL, &dummy_dist_px, r_location, r_normal, SNAP_ALL);

	return ret;
}

/* FPSInfo->state */
#define FPS_RUNNING     0
#define FPS_CANCEL      1
#define FPS_CONFIRM     2

static bool initFPSInfo(bContext *C, FPSInfo *fps, wmOperator *op, const wmEvent *event)
{
	wmWindow *win = CTX_wm_window(C);
	float upvec[3]; /* tmp */
	float mat[3][3];

	fps->rv3d = CTX_wm_region_view3d(C);
	fps->v3d = CTX_wm_view3d(C);
	fps->ar = CTX_wm_region(C);
	fps->scene = CTX_data_scene(C);

#ifdef NDOF_FPS_DEBUG
	puts("\n-- fps begin --");
#endif

	/* sanity check: for rare but possible case (if lib-linking the camera fails) */
	if ((fps->rv3d->persp == RV3D_CAMOB) && (fps->v3d->camera == NULL)) {
		fps->rv3d->persp = RV3D_PERSP;
	}

	if (fps->rv3d->persp == RV3D_CAMOB && fps->v3d->camera->id.lib) {
		BKE_report(op->reports, RPT_ERROR, "Cannot navigate a camera from an external library");
		return false;
	}

	if (ED_view3d_offset_lock_check(fps->v3d, fps->rv3d)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot navigate when the view offset is locked");
		return false;
	}

	if (fps->rv3d->persp == RV3D_CAMOB && fps->v3d->camera->constraints.first) {
		BKE_report(op->reports, RPT_ERROR, "Cannot navigate an object with constraints");
		return false;
	}

	fps->state = FPS_RUNNING;
	fps->speed = 0.0f;
	fps->pan_view = false;
	fps->xlock = FPS_AXISLOCK_STATE_ACTIVE;
	fps->zlock = FPS_AXISLOCK_STATE_ACTIVE;
	fps->xlock_momentum = 0.0f;
	fps->zlock_momentum = 0.0f;
	fps->is_fast = false;
	fps->is_slow = false;

	/* user preference settings */
	fps->teleport.duration = U.navigation.teleport_duration;
	fps->mouse_sensitivity = U.navigation.mouse_sensitivity;
	SetNavigationMode(fps, U.navigation_mode);

	fps->camera_height = U.navigation.camera_height;
	fps->jump_height = U.navigation.jump_height;
	fps->speed = U.navigation.move_speed;
	fps->speed_boost = U.navigation.boost_factor;

	fps->gravity = FPS_GRAVITY_STATE_OFF;

	fps->active_directions = 0;

#ifdef NDOF_FPS_DRAW_TOOMUCH
	fps->redraw = 1;
#endif
	zero_v3(fps->dvec_prev);

	fps->timer = WM_event_add_timer(CTX_wm_manager(C), win, TIMER, 0.01f);

	copy_v2_v2_int(fps->prev_mval, event->mval);
	copy_v2_v2_int(fps->mval, event->mval);
	fps->ndof = NULL;

	fps->time_lastdraw = fps->time_lastwheel = PIL_check_seconds_timer();

	fps->draw_handle_pixel = ED_region_draw_cb_activate(fps->ar->type, drawFPSPixel, fps, REGION_DRAW_POST_PIXEL);

	fps->rv3d->rflag |= RV3D_NAVIGATING; /* so we draw the corner margins */

	/* detect whether to start with Z locking */
	upvec[0] = 1.0f;
	upvec[1] = 0.0f;
	upvec[2] = 0.0f;
	copy_m3_m4(mat, fps->rv3d->viewinv);
	mul_m3_v3(mat, upvec);
	if (fabsf(upvec[2]) < 0.1f) {
		fps->zlock = FPS_AXISLOCK_STATE_IDLE;
	}
	upvec[0] = 0;
	upvec[1] = 0;
	upvec[2] = 0;

	fps->persp_backup = fps->rv3d->persp;
	fps->dist_backup = fps->rv3d->dist;

	/* check for fpsing ortho camera - which we cant support well
	 * we _could_ also check for an ortho camera but this is easier */
	if ((fps->rv3d->persp == RV3D_CAMOB) &&
	    (fps->rv3d->is_persp == false))
	{
		((Camera *)fps->v3d->camera->data)->type = CAM_PERSP;
		fps->is_ortho_cam = true;
	}

	if (fps->rv3d->persp == RV3D_CAMOB) {
		Object *ob_back;
		if ((U.uiflag & USER_CAM_LOCK_NO_PARENT) == 0 && (fps->root_parent = fps->v3d->camera->parent)) {
			while (fps->root_parent->parent)
				fps->root_parent = fps->root_parent->parent;
			ob_back = fps->root_parent;
		}
		else {
			ob_back = fps->v3d->camera;
		}

		/* store the original camera loc and rot */
		fps->obtfm = BKE_object_tfm_backup(ob_back);

		BKE_object_where_is_calc(fps->scene, fps->v3d->camera);
		negate_v3_v3(fps->rv3d->ofs, fps->v3d->camera->obmat[3]);

		fps->rv3d->dist = 0.0;
	}
	else {
		/* perspective or ortho */
		if (fps->rv3d->persp == RV3D_ORTHO)
			fps->rv3d->persp = RV3D_PERSP;  /* if ortho projection, make perspective */

		copy_qt_qt(fps->rot_backup, fps->rv3d->viewquat);
		copy_v3_v3(fps->ofs_backup, fps->rv3d->ofs);

		/* the dist defines a vector that is infront of the offset
		 * to rotate the view about.
		 * this is no good for fps mode because we
		 * want to rotate about the viewers center.
		 * but to correct the dist removal we must
		 * alter offset so the view doesn't jump. */

		fps->rv3d->dist = 0.0f;

		upvec[2] = fps->dist_backup; /* x and y are 0 */
		mul_m3_v3(mat, upvec);
		sub_v3_v3(fps->rv3d->ofs, upvec);
		/* Done with correcting for the dist */
	}

	ED_view3d_to_m4(fps->view_mat_prev, fps->rv3d->ofs, fps->rv3d->viewquat, fps->rv3d->dist);

	/* remove the mouse cursor temporarily */
	WM_cursor_modal_set(win, CURSOR_NONE);

	return 1;
}

static int fpsEnd(bContext *C, FPSInfo *fps)
{
	RegionView3D *rv3d = fps->rv3d;
	View3D *v3d = fps->v3d;

	float upvec[3];

	if (fps->state == FPS_RUNNING)
		return OPERATOR_RUNNING_MODAL;

#ifdef NDOF_FPS_DEBUG
	puts("\n-- fps end --");
#endif

	WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), fps->timer);

	ED_region_draw_cb_exit(fps->ar->type, fps->draw_handle_pixel);

	rv3d->dist = fps->dist_backup;
	if (fps->state == FPS_CANCEL) {
		/* Revert to original view? */
		if (fps->persp_backup == RV3D_CAMOB) { /* a camera view */
			Object *ob_back;
			ob_back = (fps->root_parent) ? fps->root_parent : fps->v3d->camera;

			/* store the original camera loc and rot */
			BKE_object_tfm_restore(ob_back, fps->obtfm);

			DAG_id_tag_update(&ob_back->id, OB_RECALC_OB);
		}
		else {
			/* Non Camera we need to reset the view back to the original location bacause the user canceled*/
			copy_qt_qt(rv3d->viewquat, fps->rot_backup);
			rv3d->persp = fps->persp_backup;
		}
		/* always, is set to zero otherwise */
		copy_v3_v3(rv3d->ofs, fps->ofs_backup);
	}
	else if (fps->persp_backup == RV3D_CAMOB) { /* camera */
		DAG_id_tag_update(fps->root_parent ? &fps->root_parent->id : &v3d->camera->id, OB_RECALC_OB);
		
		/* always, is set to zero otherwise */
		copy_v3_v3(rv3d->ofs, fps->ofs_backup);
	}
	else { /* not camera */

		/* Apply the fps mode view */
		/* restore the dist */
		float mat[3][3];
		upvec[0] = upvec[1] = 0;
		upvec[2] = fps->dist_backup; /* x and y are 0 */
		copy_m3_m4(mat, rv3d->viewinv);
		mul_m3_v3(mat, upvec);
		add_v3_v3(rv3d->ofs, upvec);
		/* Done with correcting for the dist */
	}

	if (fps->is_ortho_cam) {
		((Camera *)fps->v3d->camera->data)->type = CAM_ORTHO;
	}

	rv3d->rflag &= ~RV3D_NAVIGATING;
//XXX2.5	BIF_view3d_previewrender_signal(fps->sa, PR_DBASE|PR_DISPRECT); /* not working at the moment not sure why */

	if (fps->obtfm)
		MEM_freeN(fps->obtfm);
	if (fps->ndof)
		MEM_freeN(fps->ndof);

	/* restore the cursor */
	WM_cursor_modal_restore(CTX_wm_window(C));

	/* center the mouse */
	WM_cursor_warp(CTX_wm_window(C), fps->ar->winrct.xmin + fps->ar->winx / 2, fps->ar->winrct.ymin + fps->ar->winy / 2);

	/* garbage collection */
	MEM_freeN(fps);

	if (fps->state == FPS_CONFIRM)
		return OPERATOR_FINISHED;

	return OPERATOR_CANCELLED;
}

static void fpsEvent(bContext *C, wmOperator *UNUSED(op), FPSInfo *fps, const wmEvent *event)
{
	if (event->type == TIMER && event->customdata == fps->timer) {
		fps->redraw = 1;
	}
	else if (event->type == MOUSEMOVE) {
		copy_v2_v2_int(fps->mval, event->mval);
	}
	else if (event->type == NDOF_MOTION) {
		/* do these automagically get delivered? yes. */
		// puts("ndof motion detected in fps mode!");
		// static const char *tag_name = "3D mouse position";

		wmNDOFMotionData *incoming_ndof = (wmNDOFMotionData *)event->customdata;
		switch (incoming_ndof->progress) {
			case P_STARTING:
				/* start keeping track of 3D mouse position */
#ifdef NDOF_FPS_DEBUG
				puts("start keeping track of 3D mouse position");
#endif
				/* fall-through */
			case P_IN_PROGRESS:
				/* update 3D mouse position */
#ifdef NDOF_FPS_DEBUG
				putchar('.'); fflush(stdout);
#endif
				if (fps->ndof == NULL) {
					// fps->ndof = MEM_mallocN(sizeof(wmNDOFMotionData), tag_name);
					fps->ndof = MEM_dupallocN(incoming_ndof);
					// fps->ndof = malloc(sizeof(wmNDOFMotionData));
				}
				else {
					memcpy(fps->ndof, incoming_ndof, sizeof(wmNDOFMotionData));
				}
				break;
			case P_FINISHING:
				/* stop keeping track of 3D mouse position */
#ifdef NDOF_FPS_DEBUG
				puts("stop keeping track of 3D mouse position");
#endif
				if (fps->ndof) {
					MEM_freeN(fps->ndof);
					// free(fps->ndof);
					fps->ndof = NULL;
				}
				{
				/* update the time else the view will jump when 2D mouse/timer resume */
				fps->time_lastdraw = PIL_check_seconds_timer();
				}
				break;
			default:
				break; /* should always be one of the above 3 */
		}
	}
	/* handle modal keymap first */
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case FPS_MODAL_CANCEL:
				fps->state = FPS_CANCEL;
				break;
			case FPS_MODAL_CONFIRM:
				fps->state = FPS_CONFIRM;
				break;

			/* implement WASD keys */
			case FPS_MODAL_DIR_FORWARD:
				fps->active_directions |= FPS_BIT_FORWARD;
				break;
			case FPS_MODAL_DIR_BACKWARD:
				fps->active_directions |= FPS_BIT_BACKWARD;
				break;
			case FPS_MODAL_DIR_LEFT:
				fps->active_directions |= FPS_BIT_LEFT;
				break;
			case FPS_MODAL_DIR_RIGHT:
				fps->active_directions |= FPS_BIT_RIGHT;
				break;
			case FPS_MODAL_DIR_UP:
				fps->active_directions |= FPS_BIT_UP;
				break;
			case FPS_MODAL_DIR_DOWN:
				fps->active_directions |= FPS_BIT_DOWN;
				break;

			case FPS_MODAL_DIR_FORWARD_STOP:
				fps->active_directions &= ~FPS_BIT_FORWARD;
				break;
			case FPS_MODAL_DIR_BACKWARD_STOP:
				fps->active_directions &= ~FPS_BIT_BACKWARD;
				break;
			case FPS_MODAL_DIR_LEFT_STOP:
				fps->active_directions &= ~FPS_BIT_LEFT;
				break;
			case FPS_MODAL_DIR_RIGHT_STOP:
				fps->active_directions &= ~FPS_BIT_RIGHT;
				break;
			case FPS_MODAL_DIR_UP_STOP:
				fps->active_directions &= ~FPS_BIT_UP;
				break;
			case FPS_MODAL_DIR_DOWN_STOP:
				fps->active_directions &= ~FPS_BIT_DOWN;
				break;

			case FPS_MODAL_FAST_ENABLE:
				fps->is_fast = true;
				break;
			case FPS_MODAL_FAST_DISABLE:
				fps->is_fast = false;
				break;
			case FPS_MODAL_SLOW_ENABLE:
				fps->is_slow = true;
				break;
			case FPS_MODAL_SLOW_DISABLE:
				fps->is_slow = false;
				break;

#define JUMP_SPEED_MIN 1.f
#define JUMP_TIME_MAX 0.2f /* s */
#define JUMP_MAX_HEIGHT fps->jump_height /* m */
#define JUMP_SPEED_MAX sqrt(2 * FPS_GRAVITY * JUMP_MAX_HEIGHT)
			case FPS_MODAL_JUMP_STOP:
				if (fps->gravity == FPS_GRAVITY_STATE_JUMP) {
					double t;

					/* delta time */
					t = PIL_check_seconds_timer() - fps->teleport.initial_time;

					/* reduce the veolocity, if JUMP wasn't hold for long enough */
					t = min_ff(t, JUMP_TIME_MAX);
					fps->speed_jump = JUMP_SPEED_MIN + t * (JUMP_SPEED_MAX - JUMP_SPEED_MIN) / JUMP_TIME_MAX;

					/* when jumping, duration is how long it takes before we start going down */
					fps->teleport.duration = getVelocityZeroTime(fps->speed_jump);

					/* no more increase of jump speed */
					fps->gravity = FPS_GRAVITY_STATE_ON;
				}
				break;
			case FPS_MODAL_JUMP:
				if ((fps->navigation_mode == FPS_NAVIGATION_WALK) &&
					(fps->gravity == FPS_GRAVITY_STATE_OFF) &&
					fps->teleport.state == FPS_TELEPORT_STATE_OFF) {
					/* no need to check for ground,
					 * fps->gravity wouldn't be off
					 * if we were over a hole */
					fps->gravity = FPS_GRAVITY_STATE_JUMP;
					fps->speed_jump = JUMP_SPEED_MAX;

					fps->teleport.initial_time = PIL_check_seconds_timer();
					copy_v3_v3(fps->teleport.origin, fps->rv3d->viewinv[3]);

					/* using previous vec because WASD keys are not called when SPACE is */
					copy_v2_v2(fps->teleport.direction, fps->dvec_prev);

					/* when jumping, duration is how long it takes before we start going down */
					fps->teleport.duration = getVelocityZeroTime(fps->speed_jump);
				}

				if (fps->navigation_mode == FPS_NAVIGATION_WALK) {
					break;
				}
				else {
					// do not break, teleport
				}
			case FPS_MODAL_TELEPORT:
			{
				float loc[3], nor[3];
				float distance;
				bool ret = getTeleportRay(C, fps->rv3d, loc, nor, &distance);

				/* in case we are teleporting middle way from a jump */
				fps->speed_jump = 0.f;

				if (ret) {
					FPSTeleport *teleport = &fps->teleport;
					teleport->state = FPS_TELEPORT_STATE_ON;
					teleport->initial_time = PIL_check_seconds_timer();
					teleport->duration = U.navigation.teleport_duration;

					teleport->navigation_mode = fps->navigation_mode;
					SetNavigationMode(fps, FPS_NAVIGATION_FLY);

					copy_v3_v3(teleport->origin, fps->rv3d->viewinv[3]);
					sub_v3_v3v3(teleport->direction, loc, teleport->origin);

					/* XXX TODO
					 we could have an offset so we don't move all the way to the destination
					 if WALK we should keep the same original height (or leave it to after the fact)
					 */
				}
				else {
					fps->teleport.state = FPS_TELEPORT_STATE_OFF;
				}
				break;
			}

#undef JUMP_SPEED_MAX
#undef JUMP_MAX_HEIGHT
#undef JUMP_TIME_MAX
#undef JUMP_SPEED_MIN

			case FPS_MODAL_AXIS_LOCK_X:
				if (fps->xlock != FPS_AXISLOCK_STATE_OFF)
					fps->xlock = FPS_AXISLOCK_STATE_OFF;
				else {
					fps->xlock = FPS_AXISLOCK_STATE_ACTIVE;
					fps->xlock_momentum = 0.0;
				}
				break;
			case FPS_MODAL_AXIS_LOCK_Z:
				if (fps->zlock != FPS_AXISLOCK_STATE_OFF)
					fps->zlock = FPS_AXISLOCK_STATE_OFF;
				else {
					fps->zlock = FPS_AXISLOCK_STATE_ACTIVE;
					fps->zlock_momentum = 0.0;
				}
				break;

			case FPS_MODAL_WALK:
				SetNavigationMode(fps, FPS_NAVIGATION_WALK);
				break;
			case FPS_MODAL_FLY:
				SetNavigationMode(fps, FPS_NAVIGATION_FLY);
				break;
			case FPS_MODAL_TOGGLE:
				if (fps->navigation_mode == FPS_NAVIGATION_WALK) {
					SetNavigationMode(fps, FPS_NAVIGATION_FLY);
				}
				else { /* FPS_NAVIGATION_FLY */
					SetNavigationMode(fps, FPS_NAVIGATION_WALK);				}
				break;
		}
	}
}

static void fpsMoveCamera(bContext *C, RegionView3D *rv3d, FPSInfo *fps,
                            const bool do_rotate, const bool do_translate)
{
	/* we are in camera view so apply the view ofs and quat to the view matrix and set the camera to the view */

	View3D *v3d = fps->v3d;
	Scene *scene = fps->scene;
	ID *id_key;

	/* transform the parent or the camera? */
	if (fps->root_parent) {
		Object *ob_update;

		float view_mat[4][4];
		float prev_view_imat[4][4];
		float diff_mat[4][4];
		float parent_mat[4][4];

		invert_m4_m4(prev_view_imat, fps->view_mat_prev);
		ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);
		mul_m4_m4m4(diff_mat, view_mat, prev_view_imat);
		mul_m4_m4m4(parent_mat, diff_mat, fps->root_parent->obmat);

		BKE_object_apply_mat4(fps->root_parent, parent_mat, true, false);

		// BKE_object_where_is_calc(scene, fps->root_parent);

		ob_update = v3d->camera->parent;
		while (ob_update) {
			DAG_id_tag_update(&ob_update->id, OB_RECALC_OB);
			ob_update = ob_update->parent;
		}

		copy_m4_m4(fps->view_mat_prev, view_mat);

		id_key = &fps->root_parent->id;
	}
	else {
		float view_mat[4][4];
		float size_mat[4][4];
		float size_back[3];

		/* even though we handle the size matrix, this still changes over time */
		copy_v3_v3(size_back, v3d->camera->size);

		ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);
		size_to_mat4(size_mat, v3d->camera->size);
		mul_m4_m4m4(view_mat, view_mat, size_mat);

		BKE_object_apply_mat4(v3d->camera, view_mat, true, true);

		copy_v3_v3(v3d->camera->size, size_back);

		id_key = &v3d->camera->id;
	}

	/* record the motion */
	if (autokeyframe_cfra_can_key(scene, id_key)) {
		ListBase dsources = {NULL, NULL};

		/* add datasource override for the camera object */
		ANIM_relative_keyingset_add_source(&dsources, id_key, NULL, NULL);

		/* insert keyframes 
		 *	1) on the first frame
		 *	2) on each subsequent frame
		 *		TODO: need to check in future that frame changed before doing this 
		 */
		if (do_rotate) {
			KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_ROTATION_ID);
			ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
		}
		if (do_translate) {
			KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOCATION_ID);
			ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
		}

		/* free temp data */
		BLI_freelistN(&dsources);
	}
}

static float getFreeFallDistance(double time)
{
	return FPS_GRAVITY * (time * time) * 0.5;
}

static float getVelocityZeroTime(float velocity) {
	return velocity / FPS_GRAVITY;
}

static int fpsApply(bContext *C, FPSInfo *fps)
{
#define FPS_ROTATE_FAC 0.8f /* more is faster */
#define FPS_ZUP_CORRECT_FAC 0.1f /* amount to correct per step */
#define FPS_ZUP_CORRECT_ACCEL 0.05f /* increase upright momentum each step */
#define FPS_SMOOTH_FAC 20.0f  /* higher value less lag */
#define FPS_TOP_LIMIT DEG2RADF(85.0f)
#define FPS_BOTTOM_LIMIT DEG2RADF(-80.0f)
#define FPS_MOVE_SPEED U.navigation.move_speed
#define FPS_BOOST_FACTOR fps->speed_boost
#define TELEPORT_OFFSET 0.8f

	/* fps mode - Ctrl+Shift+F
	 * a fps loop where the user can move move the view as if they are in a fps game
	 */
	RegionView3D *rv3d = fps->rv3d;
	ARegion *ar = fps->ar;

	float mat[3][3]; /* 3x3 copy of the view matrix so we can move along the view axis */
	float dvec[3] = {0, 0, 0}; /* this is the direction thast added to the view offset per redraw */

	/* Camera Uprighting variables */
	float upvec[3] = {0, 0, 0}; /* stores the view's up vector */

	float moffset[2]; /* mouse offset from the views center */
	float tmp_quat[4]; /* used for rotating the view */

#ifdef NDOF_FPS_DEBUG
	{
		static unsigned int iteration = 1;
		printf("fps timer %d\n", iteration++);
	}
#endif

	{
		/* mouse offset from the center */
		moffset[0] = fps->mval[0] - fps->prev_mval[0];
		moffset[1] = fps->mval[1] - fps->prev_mval[1];

		copy_v2_v2_int(fps->prev_mval, fps->mval);

		/* Should we redraw? */
		if ((fps->speed != 0.0f) ||
		    moffset[0] || moffset[1] ||
		    (fps->zlock != FPS_AXISLOCK_STATE_OFF) ||
		    (fps->xlock != FPS_AXISLOCK_STATE_OFF) ||
			fps->teleport.state == FPS_TELEPORT_STATE_ON ||
			fps->gravity != FPS_GRAVITY_STATE_OFF)
		{
			float dvec_tmp[3];

			/* time how fast it takes for us to redraw,
			 * this is so simple scenes don't fps too fast */
			double time_current;
			float time_redraw;
			float time_redraw_clamped;
#ifdef NDOF_FPS_DRAW_TOOMUCH
			fps->redraw = 1;
#endif
			time_current = PIL_check_seconds_timer();
			time_redraw = (float)(time_current - fps->time_lastdraw);
			time_redraw_clamped = min_ff(0.05f, time_redraw); /* clamp redraw time to avoid jitter in roll correction */

			fps->time_lastdraw = time_current;

			/* base speed in m/s */
			fps->speed = FPS_MOVE_SPEED;

			if (fps->is_fast) {
				fps->speed *= FPS_BOOST_FACTOR;
			} else if (fps->is_slow) {
				fps->speed *= 1.f / FPS_BOOST_FACTOR;
			}

			copy_m3_m4(mat, rv3d->viewinv);

			{
				float roll; /* similar to the angle between the camera's up and the Z-up,
				             * but its very rough so just roll */

				/* rotate about the X axis- look up/down */
				if (moffset[1]) {
					float angle;
					float y;

					/* relative offset */
					y = moffset[1] / ar->winy;

					/* speed factor */
					y *= FPS_ROTATE_FAC;

					/* user adjustement factor */
					y *= fps->mouse_sensitivity;

					/* clamp the angle limits */
					/* it ranges from 90.0f to -90.0f */
					angle = -asin(rv3d->viewmat[2][2]);

					if (angle > FPS_TOP_LIMIT && y > 0)
						y = 0;

					else if (angle < FPS_BOTTOM_LIMIT && y < 0)
						y = 0;

					upvec[0] = 1;
					upvec[1] = 0;
					upvec[2] = 0;
					mul_m3_v3(mat, upvec);
					/* Rotate about the relative up vec */
					axis_angle_to_quat(tmp_quat, upvec, -y);
					mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

					if (fps->xlock != FPS_AXISLOCK_STATE_OFF)
						fps->xlock = FPS_AXISLOCK_STATE_ACTIVE;  /* check for rotation */
					if (fps->zlock != FPS_AXISLOCK_STATE_OFF)
						fps->zlock = FPS_AXISLOCK_STATE_ACTIVE;
					fps->xlock_momentum = 0.0f;
				}

				/* rotate about the Y axis- look left/right */
				if (moffset[0]) {
					float x;

					/* if we're upside down invert the moffset */
					upvec[0] = 0.0f;
					upvec[1] = 1.0f;
					upvec[2] = 0.0f;
					mul_m3_v3(mat, upvec);

					if (upvec[2] < 0.0f)
						moffset[0] = -moffset[0];

					/* relative offset */
					x = moffset[0] / ar->winx;

					/* speed factor */
					x *= FPS_ROTATE_FAC;

					/* user adjustement factor */
					x *= fps->mouse_sensitivity;

					/* make the lock vectors */
					if (fps->zlock) {
						upvec[0] = 0.0f;
						upvec[1] = 0.0f;
						upvec[2] = 1.0f;
					}
					else {
						upvec[0] = 0.0f;
						upvec[1] = 1.0f;
						upvec[2] = 0.0f;
						mul_m3_v3(mat, upvec);
					}

					/* Rotate about the relative up vec */
					axis_angle_to_quat(tmp_quat, upvec, x);
					mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

					if (fps->xlock != FPS_AXISLOCK_STATE_OFF)
						fps->xlock = FPS_AXISLOCK_STATE_ACTIVE;  /* check for rotation */
					if (fps->zlock != FPS_AXISLOCK_STATE_OFF)
						fps->zlock = FPS_AXISLOCK_STATE_ACTIVE;
				}

/* XXX axis rolling is deactive by now. They work, but they affect the behaviour
   of moving around. It needs to be rewritten to work with the fps system */
#if 0
				if (fps->zlock == FPS_AXISLOCK_STATE_ACTIVE) {
					upvec[0] = 1.0f;
					upvec[1] = 0.0f;
					upvec[2] = 0.0f;
					mul_m3_v3(mat, upvec);

					/* make sure we have some z rolling */
					if (fabsf(upvec[2]) > 0.00001f) {
						roll = upvec[2] * 5.0f;
						upvec[0] = 0.0f; /* rotate the view about this axis */
						upvec[1] = 0.0f;
						upvec[2] = 1.0f;

						mul_m3_v3(mat, upvec);
						/* Rotate about the relative up vec */
						axis_angle_to_quat(tmp_quat, upvec,
						                   roll * time_redraw_clamped * fps->zlock_momentum * FPS_ZUP_CORRECT_FAC);
						mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

						fps->zlock_momentum += FPS_ZUP_CORRECT_ACCEL;
					}
					else {
						printf("done rotating zlock\n");
						fps->zlock = FPS_AXISLOCK_STATE_OFF; /* don't check until the view rotates again */
						fps->zlock_momentum = 0.0f;
					}
				}

				/* only apply xcorrect when mouse isn't applying x rot */
				if (fps->xlock == FPS_AXISLOCK_STATE_ACTIVE && moffset[1] == 0) {
					upvec[0] = 0;
					upvec[1] = 0;
					upvec[2] = 1;
					mul_m3_v3(mat, upvec);
					/* make sure we have some z rolling */
					if (fabsf(upvec[2]) > 0.00001f) {
						roll = upvec[2] * -5.0f;

						upvec[0] = 1.0f; /* rotate the view about this axis */
						upvec[1] = 0.0f;
						upvec[2] = 0.0f;

						mul_m3_v3(mat, upvec);

						/* Rotate about the relative up vec */
						axis_angle_to_quat(tmp_quat, upvec, roll * time_redraw_clamped * fps->xlock_momentum * 0.1f);
						mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

						fps->xlock_momentum += 0.05f;
					}
					else {
						printf("done rotating xlock\n");
						fps->xlock = FPS_AXISLOCK_STATE_OFF; /* see above */
						fps->xlock_momentum = 0.0f;
					}
				}
#endif
			}

			/* WASD - 'move' translation code */
			if ((fps->active_directions) &&
				fps->gravity == FPS_GRAVITY_STATE_OFF){

				short direction;
				zero_v3(dvec);

				if ((fps->active_directions & FPS_BIT_FORWARD) ||
					(fps->active_directions & FPS_BIT_BACKWARD)) {

					direction = 0;

					if ((fps->active_directions & FPS_BIT_FORWARD))
						direction += 1;

					if ((fps->active_directions & FPS_BIT_BACKWARD))
						direction -= 1;

					if (fps->navigation_mode == FPS_NAVIGATION_WALK) {
						dvec_tmp[0] = direction * rv3d->viewinv[1][0];
						dvec_tmp[1] = direction * rv3d->viewinv[1][1];
						dvec_tmp[2] = 0.0f;

						if (rv3d->viewmat[2][2] > 0) {
							mul_v3_fl(dvec_tmp, -1.0);
						}
					}
					else { /* FPS_NAVIGATION_FLY */
						dvec_tmp[0] = 0.0f;
						dvec_tmp[1] = 0.0f;
						dvec_tmp[2] = 1.0f;
						mul_m3_v3(mat, dvec_tmp);

						mul_v3_fl(dvec_tmp, direction);
					}

					normalize_v3(dvec_tmp);
					add_v3_v3(dvec, dvec_tmp);

				}

				if ((fps->active_directions & FPS_BIT_LEFT) ||
					(fps->active_directions & FPS_BIT_RIGHT)) {

					direction = 0;

					if ((fps->active_directions & FPS_BIT_LEFT))
						direction += 1;

					if ((fps->active_directions & FPS_BIT_RIGHT))
						direction -= 1;

					dvec_tmp[0] = direction * rv3d->viewinv[0][0];
					dvec_tmp[1] = direction * rv3d->viewinv[0][1];
					dvec_tmp[2] = 0.0f;

					normalize_v3(dvec_tmp);
					add_v3_v3(dvec, dvec_tmp);

				}

				if ((fps->active_directions & FPS_BIT_UP) ||
				    (fps->active_directions & FPS_BIT_DOWN)) {

					direction = 0;

					if ((fps->active_directions & FPS_BIT_UP))
						direction -= 1;

					if ((fps->active_directions & FPS_BIT_DOWN))
						direction = 1;

					dvec_tmp[0] = 0.0;
					dvec_tmp[1] = 0.0;
					dvec_tmp[2] = direction;

					add_v3_v3(dvec, dvec_tmp);
				}

				mul_v3_fl(dvec, fps->speed * time_redraw);
			}

			/* stick to the floor */
			if (fps->navigation_mode == FPS_NAVIGATION_WALK &&
				ELEM(fps->gravity,
				      FPS_GRAVITY_STATE_OFF,
				      FPS_GRAVITY_STATE_START)) {

				if ((fps->active_directions & FPS_BIT_UP) ||
					(fps->active_directions & FPS_BIT_DOWN)){
					fps->camera_height -= dvec[2];
				}
				else {
					bool ret;
					float ray_distance;
					float difference = -100.f;
					float fall_distance;

					ret = getFloorDistance(C, rv3d, dvec, &ray_distance);

					if (ret) {
						difference = fps->camera_height - ray_distance;
					}

					/* the distance we would fall naturally smoothly enough that we
					   can manually drop the object without activating gravity */
					fall_distance = time_redraw * fps->speed * FPS_BOOST_FACTOR;

					if (abs(difference) < fall_distance) {
						/* slope/stairs */
						dvec[2] -= difference;
					}
					else {
						/* hijack the teleport variables */
						fps->teleport.initial_time = PIL_check_seconds_timer();
						fps->gravity = FPS_GRAVITY_STATE_ON;
						fps->teleport.duration = 0;

						copy_v3_v3(fps->teleport.origin, fps->rv3d->viewinv[3]);
						copy_v2_v2(fps->teleport.direction, dvec);

					}
				}
			}

			/* Falling or jumping) */
			if (ELEM(fps->gravity, FPS_GRAVITY_STATE_ON, FPS_GRAVITY_STATE_JUMP)) {
				double t;
				float cur_zed, new_zed;
				bool ret;
				float ray_distance, difference = -100.f;

				/* delta time */
				t = PIL_check_seconds_timer() - fps->teleport.initial_time;

				/* keep moving if we were moving */
				copy_v2_v2(dvec, fps->teleport.direction);

				cur_zed = fps->rv3d->viewinv[3][2];
				new_zed = fps->teleport.origin[2] - getFreeFallDistance(t);

				/* jump */
				new_zed += t * fps->speed_jump;

				dvec[2] = cur_zed - new_zed;

				/* duration is the jump duration */
				if (t > fps->teleport.duration) {
					/* check to see if we are landing */
					ret = getFloorDistance(C, rv3d, dvec, &ray_distance);

					if (ret) {
						difference = fps->camera_height - ray_distance;
					}

					if (ray_distance < fps->camera_height) {
						/* quit falling */
						dvec[2] -= difference;
						fps->gravity = FPS_GRAVITY_STATE_OFF;
						fps->speed_jump = 0.f;
					}
				}
			}

			/* Teleport */
			else if (fps->teleport.state == FPS_TELEPORT_STATE_ON) {
				double t; /* factor */
				float new_loc[3];
				float cur_loc[3];

				/* linear interpolation */
				t = PIL_check_seconds_timer() - fps->teleport.initial_time;
				t /= fps->teleport.duration;

				/* clamp so we don't go past our limit */
				if (t >= TELEPORT_OFFSET) {
					t = TELEPORT_OFFSET;
					fps->teleport.state = FPS_TELEPORT_STATE_OFF;
					SetNavigationMode(fps, fps->teleport.navigation_mode);
				}

				mul_v3_v3fl(new_loc, fps->teleport.direction, t);
				add_v3_v3(new_loc, fps->teleport.origin);

				copy_v3_v3(cur_loc, fps->rv3d->viewinv[3]);
				sub_v3_v3v3(dvec, cur_loc, new_loc);
			}

			if (rv3d->persp == RV3D_CAMOB) {
				Object *lock_ob = fps->root_parent ? fps->root_parent : fps->v3d->camera;
				if (lock_ob->protectflag & OB_LOCK_LOCX) dvec[0] = 0.0;
				if (lock_ob->protectflag & OB_LOCK_LOCY) dvec[1] = 0.0;
				if (lock_ob->protectflag & OB_LOCK_LOCZ) dvec[2] = 0.0;
			}

			add_v3_v3(rv3d->ofs, dvec);

			if (rv3d->persp == RV3D_CAMOB) {
				const bool do_rotate = ((fps->xlock != FPS_AXISLOCK_STATE_OFF) ||
				                        (fps->zlock != FPS_AXISLOCK_STATE_OFF) ||
				                        ((moffset[0] || moffset[1]) && !fps->pan_view));
				const bool do_translate = (fps->speed != 0.0f || fps->pan_view);
				fpsMoveCamera(C, rv3d, fps, do_rotate, do_translate);
			}

		}
		else {
			/* we're not redrawing but we need to update the time else the view will jump */
			fps->time_lastdraw = PIL_check_seconds_timer();
		}
		/* end drawing */
		copy_v3_v3(fps->dvec_prev, dvec);
	}

	return OPERATOR_FINISHED;
#undef FPS_ROTATE_FAC
#undef FPS_ZUP_CORRECT_FAC
#undef FPS_ZUP_CORRECT_ACCEL
#undef FPS_SMOOTH_FAC
#undef FPS_TOP_LIMIT
#undef FPS_BOTTOM_LIMIT
#undef FPS_MOVE_SPEED
#undef FPS_BOOST_FACTOR
#undef TELEPORT_OFFSET
}

static int fpsApply_ndof(bContext *C, FPSInfo *fps)
{
	/* shorthand for oft-used variables */
	wmNDOFMotionData *ndof = fps->ndof;
	const float dt = ndof->dt;
	RegionView3D *rv3d = fps->rv3d;
	const int flag = U.ndof_flag;

#if 0
	bool do_rotate = (flag & NDOF_SHOULD_ROTATE) && (fps->pan_view == false);
	bool do_translate = (flag & (NDOF_SHOULD_PAN | NDOF_SHOULD_ZOOM)) != 0;
#endif

	bool do_rotate = (fps->pan_view == false);
	bool do_translate = true;

	float view_inv[4];
	invert_qt_qt(view_inv, rv3d->viewquat);

	rv3d->rot_angle = 0.0f; /* disable onscreen rotation doo-dad */

	if (do_translate) {
		const float forward_sensitivity  = 1.0f;
		const float vertical_sensitivity = 0.4f;
		const float lateral_sensitivity  = 0.6f;

		float speed = 10.0f; /* blender units per second */
		/* ^^ this is ok for default cube scene, but should scale with.. something */

		float trans[3] = {lateral_sensitivity  * ndof->tvec[0],
		                  vertical_sensitivity * ndof->tvec[1],
		                  forward_sensitivity  * ndof->tvec[2]};

		if (fps->is_slow)
			speed *= 0.2f;

		mul_v3_fl(trans, speed * dt);

		/* transform motion from view to world coordinates */
		mul_qt_v3(view_inv, trans);

		if (flag & NDOF_FLY_HELICOPTER) {
			/* replace world z component with device y (yes it makes sense) */
			trans[2] = speed * dt * vertical_sensitivity * ndof->tvec[1];
		}

		if (rv3d->persp == RV3D_CAMOB) {
			/* respect camera position locks */
			Object *lock_ob = fps->root_parent ? fps->root_parent : fps->v3d->camera;
			if (lock_ob->protectflag & OB_LOCK_LOCX) trans[0] = 0.0f;
			if (lock_ob->protectflag & OB_LOCK_LOCY) trans[1] = 0.0f;
			if (lock_ob->protectflag & OB_LOCK_LOCZ) trans[2] = 0.0f;
		}

		if (!is_zero_v3(trans)) {
			/* move center of view opposite of hand motion (this is camera mode, not object mode) */
			sub_v3_v3(rv3d->ofs, trans);
			do_translate = true;
		}
		else {
			do_translate = false;
		}
	}

	if (do_rotate) {
		const float turn_sensitivity = 1.0f;

		float rotation[4];
		float axis[3];
		float angle = turn_sensitivity * ndof_to_axis_angle(ndof, axis);

		if (fabsf(angle) > 0.0001f) {
			do_rotate = true;

			if (fps->is_slow)
				angle *= 0.2f;

			/* transform rotation axis from view to world coordinates */
			mul_qt_v3(view_inv, axis);

			/* apply rotation to view */
			axis_angle_to_quat(rotation, axis, angle);
			mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);

			if (flag & NDOF_LOCK_HORIZON) {
				/* force an upright viewpoint
				 * TODO: make this less... sudden */
				float view_horizon[3] = {1.0f, 0.0f, 0.0f}; /* view +x */
				float view_direction[3] = {0.0f, 0.0f, -1.0f}; /* view -z (into screen) */

				/* find new inverse since viewquat has changed */
				invert_qt_qt(view_inv, rv3d->viewquat);
				/* could apply reverse rotation to existing view_inv to save a few cycles */

				/* transform view vectors to world coordinates */
				mul_qt_v3(view_inv, view_horizon);
				mul_qt_v3(view_inv, view_direction);

				/* find difference between view & world horizons
				 * true horizon lives in world xy plane, so look only at difference in z */
				angle = -asinf(view_horizon[2]);

#ifdef NDOF_FPS_DEBUG
				printf("lock horizon: adjusting %.1f degrees\n\n", RAD2DEG(angle));
#endif

				/* rotate view so view horizon = world horizon */
				axis_angle_to_quat(rotation, view_direction, angle);
				mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);
			}

			rv3d->view = RV3D_VIEW_USER;
		}
		else {
			do_rotate = false;
		}
	}

	if (do_translate || do_rotate) {
		fps->redraw = true;

		if (rv3d->persp == RV3D_CAMOB) {
			fpsMoveCamera(C, rv3d, fps, do_rotate, do_translate);
		}
	}

	return OPERATOR_FINISHED;
}

static int fps_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	FPSInfo *fps;

	if (rv3d->viewlock)
		return OPERATOR_CANCELLED;

	fps = MEM_callocN(sizeof(FPSInfo), "FPSOperation");

	op->customdata = fps;

	if (initFPSInfo(C, fps, op, event) == false) {
		MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}

	fpsEvent(C, op, fps, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static void fps_cancel(bContext *C, wmOperator *op)
{
	FPSInfo *fps = op->customdata;

	fps->state = FPS_CANCEL;
	fpsEnd(C, fps);
	op->customdata = NULL;
}

static int fps_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	int exit_code;
	bool do_draw = false;
	FPSInfo *fps = op->customdata;
	RegionView3D *rv3d = fps->rv3d;
	Object *fps_object = fps->root_parent ? fps->root_parent : fps->v3d->camera;

	fps->redraw = 0;

	fpsEvent(C, op, fps, event);

	if (fps->ndof) { /* 3D mouse overrules [2D mouse + timer] */
		if (event->type == NDOF_MOTION) {
			fpsApply_ndof(C, fps);
		}
	}
	else if (event->type == TIMER && event->customdata == fps->timer) {
		fpsApply(C, fps);
	}

	do_draw |= fps->redraw;

	exit_code = fpsEnd(C, fps);

	if (exit_code != OPERATOR_RUNNING_MODAL)
		do_draw = true;

	if (do_draw) {
		if (rv3d->persp == RV3D_CAMOB) {
			WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, fps_object);
		}

		// puts("redraw!"); // too frequent, commented with NDOF_FPS_DRAW_TOOMUCH for now
		ED_region_tag_redraw(CTX_wm_region(C));
	}

	return exit_code;
}

void VIEW3D_OT_fps(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "FPS Navigation";
	ot->description = "Interactively navigate around the scene";
	ot->idname = "VIEW3D_OT_fps";

	/* api callbacks */
	ot->invoke = fps_invoke;
	ot->cancel = fps_cancel;
	ot->modal = fps_modal;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_CURSOR_WRAP;

	RNA_def_boolean(ot->srna, "use_vertical_restrict", 1, "Restrict Vertical View", "Restrict view from looking above human limits");
	RNA_def_enum(ot->srna, "type", view_navigation_items, 0, "Navigation Type", "");
}

#undef FPS_GRAVITY
