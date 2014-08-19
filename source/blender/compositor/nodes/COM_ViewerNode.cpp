/*
 * Copyright 2011, Blender Foundation.
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
 */

#include "COM_ViewerNode.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BLI_listbase.h"

#include "COM_ViewerOperation.h"
#include "COM_ExecutionSystem.h"

ViewerNode::ViewerNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

static size_t ViewerNodeViewsCount(const RenderData *rd)
{
	SceneRenderView *srv;
	size_t totviews	= 0;

	for (srv = (SceneRenderView *)rd->views.first; srv; srv = srv->next)
		if ((srv->viewflag & SCE_VIEW_DISABLE) == 0)
			totviews++;
	return totviews;
}

static bool ViewerNodeIsStereo(const RenderData *rd)
{
	SceneRenderView *srv[2];

	if ((rd->scemode & R_MULTIVIEW) == 0)
		return false;

	srv[0] = (SceneRenderView *)BLI_findstring(&rd->views, STEREO_LEFT_NAME, offsetof(SceneRenderView, name));
	srv[1] = (SceneRenderView *)BLI_findstring(&rd->views, STEREO_RIGHT_NAME, offsetof(SceneRenderView, name));

	return (srv[0] && ((srv[0]->viewflag & SCE_VIEW_DISABLE) == 0) &&
	        srv[1] && ((srv[1]->viewflag & SCE_VIEW_DISABLE) == 0));
}

void ViewerNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorNode = this->getbNode();
	bool do_output = (editorNode->flag & NODE_DO_OUTPUT_RECALC || context.isRendering()) && (editorNode->flag & NODE_DO_OUTPUT);
	bool ignore_alpha = editorNode->custom2 & CMP_NODE_OUTPUT_IGNORE_ALPHA;

	NodeInput *imageSocket = this->getInputSocket(0);
	NodeInput *alphaSocket = this->getInputSocket(1);
	NodeInput *depthSocket = this->getInputSocket(2);
	Image *image = (Image *)this->getbNode()->id;
	ImageUser *imageUser = (ImageUser *) this->getbNode()->storage;
	ViewerOperation *viewerOperation = new ViewerOperation();
	viewerOperation->setbNodeTree(context.getbNodeTree());
	viewerOperation->setImage(image);
	viewerOperation->setImageUser(imageUser);
	viewerOperation->setChunkOrder((OrderOfChunks)editorNode->custom1);
	viewerOperation->setCenterX(editorNode->custom3);
	viewerOperation->setCenterY(editorNode->custom4);
	/* alpha socket gives either 1 or a custom alpha value if "use alpha" is enabled */
	viewerOperation->setUseAlphaInput(ignore_alpha || alphaSocket->isLinked());
	viewerOperation->setViewId(context.getViewId());

	viewerOperation->setViewSettings(context.getViewSettings());
	viewerOperation->setDisplaySettings(context.getDisplaySettings());

	viewerOperation->setResolutionInputSocketIndex(0);
	if (!imageSocket->isLinked()) {
		if (alphaSocket->isLinked()) {
			viewerOperation->setResolutionInputSocketIndex(1);
		}
	}

	converter.addOperation(viewerOperation);
	converter.mapInputSocket(imageSocket, viewerOperation->getInputSocket(0));
	/* only use alpha link if "use alpha" is enabled */
	if (ignore_alpha)
		converter.addInputValue(viewerOperation->getInputSocket(1), 1.0f);
	else
		converter.mapInputSocket(alphaSocket, viewerOperation->getInputSocket(1));
	converter.mapInputSocket(depthSocket, viewerOperation->getInputSocket(2));

	converter.addNodeInputPreview(imageSocket);

	if (do_output)
		converter.registerViewer(viewerOperation);

	if (image) {
		BLI_lock_thread(LOCK_DRAW_IMAGE);
		if (ViewerNodeIsStereo(context.getRenderData())) {
			image->flag |= IMA_IS_STEREO;
		}
		else {
			image->flag &= ~IMA_IS_STEREO;
			imageUser->flag &= ~IMA_SHOW_STEREO;
		}

		size_t num_views = ViewerNodeViewsCount(context.getRenderData());
		size_t num_caches = 1;

		if (num_views != num_caches) {
			BKE_image_free_cached_frames(image);
		}

		BLI_unlock_thread(LOCK_DRAW_IMAGE);
	}
}
