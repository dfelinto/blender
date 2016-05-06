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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_probe.c
 *  \ingroup edobj
 */

#include "DNA_object_types.h"

#include "BKE_context.h"

#include "BLI_listbase.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "GPU_probe.h"

#include "ED_screen.h"
#include "ED_object.h"

#include "object_intern.h"

enum {
	UPDATE_PROBE_ACTIVE           = 1,
	UPDATE_PROBE_SELECTED         = 2,
	UPDATE_PROBE_ALL              = 3,
};

static int object_probe_update_exec(bContext *C, wmOperator *op)
{
	const int mode = RNA_enum_get(op->ptr, "type");

	if (mode == UPDATE_PROBE_ACTIVE) {
		Object *ob = ED_object_context(C);
		if (ob->gpuprobe.first) {
			LinkData *link;
			for (link = ob->gpuprobe.first; link; link = link->next) {
				GPUProbe *ref = (GPUProbe *)link->data;
				GPU_probe_set_update(ref, true);
			}
		}
	}
	else if (mode == UPDATE_PROBE_SELECTED) {
		CTX_DATA_BEGIN(C, Object *, ob, selected_objects) {
			if (ob->gpuprobe.first) {
				LinkData *link;
				for (link = ob->gpuprobe.first; link; link = link->next) {
					GPUProbe *ref = (GPUProbe *)link->data;
					GPU_probe_set_update(ref, true);
				}
			}
		}
		CTX_DATA_END;
	}
	else if (mode == UPDATE_PROBE_ALL) {
		CTX_DATA_BEGIN(C, Object *, ob, editable_objects) {
			if (ob->gpuprobe.first) {
				LinkData *link;
				for (link = ob->gpuprobe.first; link; link = link->next) {
					GPUProbe *ref = (GPUProbe *)link->data;
					GPU_probe_set_update(ref, true);
				}
			}
		}
		CTX_DATA_END;
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_probe_update(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{UPDATE_PROBE_ACTIVE, "ACTIVE_PROBE", 0, "Active Probes", ""},
		{UPDATE_PROBE_SELECTED, "SELECT_PROBE", 0, "Selected Probes", ""},
		{UPDATE_PROBE_ALL, "ALL_PROBE", 0, "All Probes", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Update Probe";
	ot->description = "Update the environment captured by Probe objects";
	ot->idname = "OBJECT_OT_probe_update";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_probe_update_exec;
	ot->poll = ED_operator_object_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 1, "Type", "");
}

/* ************ Active as Probe ************** */

static int object_active_as_probe_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obt = ED_object_context(C);

	CTX_DATA_BEGIN(C, Object *, ob, selected_objects) {
		if (ob != obt) {
			if (obt->probetype == OB_PROBE_CUBEMAP || obt->probetype == OB_PROBE_PLANAR) {
				if (!ob->probetype)
					ob->probetype = OB_PROBE_OBJECT;
				ob->probe = obt;
			}
		}
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_probe_set_active_as_probe(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Active as Probe";
	ot->description = "Make selected objects use the active Probe";
	ot->idname = "OBJECT_OT_probe_set_active_as_probe";

	/* api callbacks */
	ot->exec = object_active_as_probe_exec;
	ot->poll = ED_operator_object_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}