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
#include "bake.h"

CCL_NAMESPACE_BEGIN

struct BakePixel;

/* plain copy from Blender */
typedef struct BakePixel {
	int primitive_id;
	float u, v;
	float dudx, dudy;
	float dvdx, dvdy;
} BakePixel;

//void do_bake(BL::Object b_object, const string& pass_type, BakePixel pixel_array[], int num_pixels, int depth, float pixels[]);

CCL_NAMESPACE_END

#endif /* __BAKE_H__ */

