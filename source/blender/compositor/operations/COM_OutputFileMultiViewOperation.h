/*
 * Copyright 2013, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 *		Lukas Tönne
 *		Dalai Felinto
 */

#ifndef _COM_OutputFileMultiViewOperation_h
#define _COM_OutputFileMultiViewOperation_h
#include "COM_NodeOperation.h"

#include "BLI_rect.h"
#include "BLI_path_util.h"

#include "DNA_color_types.h"

#include "intern/openexr/openexr_multi.h"

/* Writes inputs into OpenEXR multilayer channels. */
class OutputOpenExrMultiViewOperation : public OutputOpenExrMultiLayerOperation {
private:
public:
	OutputOpenExrMultiViewOperation(const RenderData *rd, const bNodeTree *tree, const char *path, char exr_codec, int actview);

	void *get_handle(const char *filename);
	void deinitExecution();
};

#endif
