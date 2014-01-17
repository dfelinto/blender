/*
 * Copyright 2011-2014 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#include "bake.h"

CCL_NAMESPACE_BEGIN

void BakeManager::device_free(Device *device, DeviceScene *dscene) {}

void BakeManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	if(progress.get_cancel()) return;

	need_update = false;
}

BakeManager::~BakeManager()
{
	if (bake_data)
		delete bake_data;
}

BakeManager::BakeManager()
{
	bake_data = NULL;
	need_update = true;
}

BakeData *BakeManager::init(const int object, const int num_pixels)
{
	bake_data = new BakeData(object, num_pixels);
	return bake_data;
}

bool BakeManager::bake(Device *device, DeviceScene *dscene, Scene *scene, PassType passtype, BakeData *bake_data, float result[])
{

	/* TODO adapt code from mesh_displace.cpp */

	return false;
}

CCL_NAMESPACE_END

