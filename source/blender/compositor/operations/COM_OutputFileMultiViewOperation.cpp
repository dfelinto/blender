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

#include "COM_OutputFileOperation.h"
#include "COM_OutputFileMultiViewOperation.h"

#include <string.h>
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DNA_color_types.h"
#include "MEM_guardedalloc.h"

extern "C" {
	#include "IMB_imbuf.h"
	#include "IMB_colormanagement.h"
	#include "IMB_imbuf_types.h"
}

OutputOpenExrMultiViewOperation::OutputOpenExrMultiViewOperation(
        const RenderData *rd, const bNodeTree *tree, const char *path, char exr_codec, int actview):
        OutputOpenExrMultiLayerOperation(rd, tree, path, exr_codec, actview)
{
}

void *OutputOpenExrMultiViewOperation::get_handle(const char* filename)
{
	unsigned int width = this->getWidth();
	unsigned int height = this->getHeight();

	if (width != 0 && height != 0) {

		void *exrhandle;
		SceneRenderView *srv;

		exrhandle = IMB_exr_get_handle_name(filename);
		if (this->m_actview > 0) return exrhandle;

		/* MV are are doing very similar in
		 * render_result.c::render_result_new
		 * it could be an external shared function */

		/* check renderdata for amount of views */
		for (srv= (SceneRenderView *) this->m_rd->views.first; srv; srv = srv->next) {

			if (srv->viewflag & SCE_VIEW_DISABLE)
				continue;

			IMB_exr_add_view(exrhandle, srv->name);

			for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
				char channelname[EXR_TOT_MAXNAME];
				BLI_strncpy(channelname, this->m_layers[i].name, sizeof(channelname) - 2);
				char *channelname_ext = channelname + strlen(channelname);

				/* create channels */
				switch (this->m_layers[i].datatype) {
					case COM_DT_VALUE:
						strcpy(channelname_ext, ".V");
						IMB_exr_add_channel(exrhandle, 0, channelname, srv->name, 1, width, NULL);
						break;
					case COM_DT_VECTOR:
						strcpy(channelname_ext, ".X");
						IMB_exr_add_channel(exrhandle, 0, channelname, srv->name, 3, 3 * width, NULL);
						strcpy(channelname_ext, ".Y");
						IMB_exr_add_channel(exrhandle, 0, channelname, srv->name, 3, 3 * width, NULL);
						strcpy(channelname_ext, ".Z");
						IMB_exr_add_channel(exrhandle, 0, channelname, srv->name, 3, 3 * width, NULL);
						break;
					case COM_DT_COLOR:
						strcpy(channelname_ext, ".R");
						IMB_exr_add_channel(exrhandle, 0, channelname, srv->name, 4, 4 * width, NULL);
						strcpy(channelname_ext, ".G");
						IMB_exr_add_channel(exrhandle, 0, channelname, srv->name, 4, 4 * width, NULL);
						strcpy(channelname_ext, ".B");
						IMB_exr_add_channel(exrhandle, 0, channelname, srv->name, 4, 4 * width, NULL);
						strcpy(channelname_ext, ".A");
						IMB_exr_add_channel(exrhandle, 0, channelname, srv->name, 4, 4 * width, NULL);
						break;
					default:
						break;
				}
			}
		}

		BLI_make_existing_file(filename);

		/* prepare the file with all the channels */
		if(IMB_exrmultiview_begin_write(exrhandle, filename, width, height, this->m_exr_codec, true) == 0)
		{
			/* TODO, get the error from openexr's exception */
			/* XXX nice way to do report? */
			printf("Error Writing Render Result, see console\n");
			IMB_exr_close(exrhandle);
		}
		else {
			/* the actual writing */
			return exrhandle;
		}
	}
	return NULL;
}

void OutputOpenExrMultiViewOperation::deinitExecution()
{
	unsigned int width = this->getWidth();
	unsigned int height = this->getHeight();

	if (width != 0 && height != 0) {
		void *exrhandle;
		char view[64];
		Main *bmain = G.main; /* TODO, have this passed along */
		char filename[FILE_MAX];

		BKE_makepicstring_from_type(filename, this->m_path, bmain->name, this->m_rd->cfra, R_IMF_IMTYPE_MULTIVIEW,
		                            (this->m_rd->scemode & R_EXTENSION), true, "");

		exrhandle = this->get_handle(filename);

		IMB_exr_get_multiView_name(exrhandle, this->m_actview, view);
		IMB_exr_clear_channels(exrhandle);

		for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
			char channelname[EXR_TOT_MAXNAME];
			BLI_strncpy(channelname, this->m_layers[i].name, sizeof(channelname) - 2);
			char *channelname_ext = channelname + strlen(channelname);

			float *buf = this->m_layers[i].outputBuffer;

			/* create channels */
			switch (this->m_layers[i].datatype) {
				case COM_DT_VALUE:
					strcpy(channelname_ext, ".V");
					IMB_exr_add_channel(exrhandle, 0, channelname, view, 1, width, buf);
					break;
				case COM_DT_VECTOR:
					strcpy(channelname_ext, ".X");
					IMB_exr_add_channel(exrhandle, 0, channelname, view, 3, 3 * width, buf);
					strcpy(channelname_ext, ".Y");
					IMB_exr_add_channel(exrhandle, 0, channelname, view, 3, 3 * width, buf + 1);
					strcpy(channelname_ext, ".Z");
					IMB_exr_add_channel(exrhandle, 0, channelname, view, 3, 3 * width, buf + 2);
					break;
				case COM_DT_COLOR:
					strcpy(channelname_ext, ".R");
					IMB_exr_add_channel(exrhandle, 0, channelname, view, 4, 4 * width, buf);
					strcpy(channelname_ext, ".G");
					IMB_exr_add_channel(exrhandle, 0, channelname, view, 4, 4 * width, buf + 1);
					strcpy(channelname_ext, ".B");
					IMB_exr_add_channel(exrhandle, 0, channelname, view, 4, 4 * width, buf + 2);
					strcpy(channelname_ext, ".A");
					IMB_exr_add_channel(exrhandle, 0, channelname, view, 4, 4 * width, buf + 3);
					break;
				default:
					break;
			}
		}

		/* the actual writing */
		IMB_exrmultiview_write_channels(exrhandle, this->m_actview);

		for (unsigned int i = 0; i < this->m_layers.size(); ++i) {
			if (this->m_layers[i].outputBuffer) {
				MEM_freeN(this->m_layers[i].outputBuffer);
				this->m_layers[i].outputBuffer = NULL;
			}
			
			this->m_layers[i].imageInput = NULL;
		}

		/* ready to close the file */
		if (this->m_actview >= IMB_exr_get_multiView_count(exrhandle) - 1)
			IMB_exr_close(exrhandle);
	}
}


/******************************** Stereo3D ******************************/

static int get_datatype_size(DataType datatype)
{
	switch (datatype) {
		case COM_DT_VALUE:  return 1;
		case COM_DT_VECTOR: return 3;
		case COM_DT_COLOR:  return 4;
		default:            return 0;
	}
}

OutputStereoOperation::OutputStereoOperation(
        const RenderData *rd, const bNodeTree *tree, DataType datatype, ImageFormatData *format, const char *path,
        const char *name, const ColorManagedViewSettings *viewSettings, const ColorManagedDisplaySettings *displaySettings,
        const int actview):
    OutputSingleLayerOperation(rd, tree, datatype, format, path, viewSettings, displaySettings, actview)
{
	BLI_strncpy(this->m_name, name, sizeof(this->m_name));
	this->m_channels = get_datatype_size(datatype);
}

void *OutputStereoOperation::get_handle(const char* filename)
{
	size_t width = this->getWidth();
	size_t height = this->getHeight();
	const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
	size_t i;

	if (width != 0 && height != 0) {
		void *exrhandle;

		exrhandle = IMB_exr_get_handle_name(filename);

		if (this->m_actview > 0) {
			return exrhandle;
		}

		IMB_exr_clear_channels(exrhandle);

		for (i = 0; i < 2; i++) {
			IMB_exr_add_view(exrhandle, names[i]);
		}

		return exrhandle;
	}
	return NULL;
}

void OutputStereoOperation::deinitExecution()
{
	unsigned int width = this->getWidth();
	unsigned int height = this->getHeight();

	if (width != 0 && height != 0) {
		void *exrhandle;
		char view[64];

		exrhandle = this->get_handle(m_path);
		IMB_exr_get_multiView_name(exrhandle, this->m_actview, view);

		float *buf = this->m_outputBuffer;

		/* populate single EXR channel with view data */
		IMB_exr_add_channel(exrhandle, NULL, m_name, view, 1, this-> m_channels * width * height, buf);

		this->m_imageInput = NULL;
		this->m_outputBuffer = NULL;

		/* create stereo ibuf */
		if (this->m_actview >= BKE_render_num_views(this->m_rd) - 1) {
			ImBuf *ibuf[3] = {NULL};
			const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
			Main *bmain = G.main; /* TODO, have this passed along */
			char filename[FILE_MAX];
			int i;

			/* get rectf from EXR */
			for (i = 0; i < 2; i++) {
				float *rectf = IMB_exr_channel_rect(exrhandle, NULL, this->m_name, names[i]);
				ibuf[i]= IMB_allocImBuf(width, height, this->m_format->planes, 0);

				ibuf[i]->channels = this->m_channels;
				ibuf[i]->rect_float = rectf;
				ibuf[i]->mall |= IB_rectfloat;
				ibuf[i]->dither = this->m_rd->dither_intensity;

				/* do colormanagement in the individual views, so it doesn't need to do in the stereo */
				IMB_colormanagement_imbuf_for_write(ibuf[i], true, false, this->m_viewSettings,
				                                    this->m_displaySettings, this->m_format);
				IMB_prepare_write_ImBuf(IMB_isfloat(ibuf[i]), ibuf[i]);
			}

			/* create stereo buffer */
			ibuf[2] = IMB_stereoImBuf(this->m_format, ibuf[0], ibuf[1]);

			BKE_makepicstring(filename, this->m_path, bmain->name, this->m_rd->cfra, this->m_format,
			                  (this->m_rd->scemode & R_EXTENSION) != 0, true, NULL);

			BKE_imbuf_write(ibuf[2], filename, this->m_format);

			/* cleanup */
			/* imbuf knows which rects are not part of ibuf */
			for (i = 0; i < 3; i++) {
				IMB_freeImBuf(ibuf[i]);
			}
			IMB_exr_close(exrhandle);
		}
	}
}
