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

#ifndef __BAKE_H__
#define __BAKE_H__

#include "util_vector.h"
#include "device.h"
#include "scene.h"
#include "session.h"

CCL_NAMESPACE_BEGIN

class BakeData {
public:
	BakeData(const int object, const int num_pixels):
	m_object(object),
	m_num_pixels(num_pixels)
	{
		m_primitive.resize(num_pixels);
		m_u.resize(num_pixels);
		m_v.resize(num_pixels);
	};
	~BakeData()
	{
		m_primitive.clear();
		m_u.clear();
		m_v.clear();
	};

	void set(int i, int prim, float u, float v)
	{
		m_primitive[i] = prim;
		m_u[i] = u;
		m_v[i] = v;
	}

private:
	int m_object;
	int m_num_pixels;
	vector<int>m_primitive;
	vector<float>m_u;
	vector<float>m_v;
};

class BakeManager {
public:

	bool need_update;

	BakeManager();
	~BakeManager();

	BakeData *init(const int object, const int num_pixels);

	bool bake(Device *device, DeviceScene *dscene, Scene *scene, PassType passtype, BakeData *bake_data, float result[]);

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);
	bool modified(const CurveSystemManager& CurveSystemManager);
	void tag_update(Scene *scene);
	void tag_update_mesh();

private:
	BakeData *bake_data;
};


//void do_bake(BL::Object b_object, const string& pass_type, BakePixel pixel_array[], int num_pixels, int depth, float pixels[]);

CCL_NAMESPACE_END

#endif /* __BAKE_H__ */

