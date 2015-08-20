/*
 * Copyright 2011-2013 Blender Foundation
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
 * limitations under the License.
 */

#include <stdlib.h>

#include "buffers.h"
#include "device.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_hash.h"
#include "util_image.h"
#include "util_math.h"
#include "util_opengl.h"
#include "util_time.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* Buffer Params */

BufferParams::BufferParams()
{
	width = 0;
	height = 0;

	full_x = 0;
	full_y = 0;
	full_width = 0;
	full_height = 0;

	Pass::add(PASS_COMBINED, passes);
	lwr_passes = 0;
}

void BufferParams::get_offset_stride(int& offset, int& stride)
{
	offset = -(full_x + full_y*width);
	stride = width;
}

bool BufferParams::modified(const BufferParams& params)
{
	return !(full_x == params.full_x
		&& full_y == params.full_y
		&& width == params.width
		&& height == params.height
		&& full_width == params.full_width
		&& full_height == params.full_height
		&& Pass::equals(passes, params.passes)
		&& lwr_passes == params.lwr_passes);
}

int BufferParams::get_passes_size(bool get_lwr)
{
	int size = 0;

	foreach(Pass& pass, passes)
		size += pass.components;

	if(get_lwr)
		return size;

	if(lwr_passes)
		size += 20;
	for(int i = 1; i < 7; i++)
		if(lwr_passes & (1 << i))
			size += 3;

	return align_up(size, 4);
}

/* Render Buffer Task */

RenderTile::RenderTile()
{
	x = 0;
	y = 0;
	w = 0;
	h = 0;

	sample = 0;
	start_sample = 0;
	num_samples = 0;
	resolution = 0;

	offset = 0;
	stride = 0;

	buffer = 0;
	rng_state = 0;

	buffers = NULL;
}

/* Render Buffers */

RenderBuffers::RenderBuffers(Device *device_)
{
	device = device_;
}

RenderBuffers::~RenderBuffers()
{
	device_free();
}

void RenderBuffers::device_free()
{
	if(buffer.device_pointer) {
		device->mem_free(buffer);
		buffer.clear();
	}

	if(rng_state.device_pointer) {
		device->mem_free(rng_state);
		rng_state.clear();
	}
}

void RenderBuffers::reset(Device *device, BufferParams& params_)
{
	params = params_;

	/* free existing buffers */
	device_free();
	
	/* allocate buffer */
	buffer.resize(params.width*params.height*params.get_passes_size());
	device->mem_alloc(buffer, MEM_READ_WRITE);
	device->mem_zero(buffer);

	/* allocate rng state */
	rng_state.resize(params.width, params.height);

	uint *init_state = rng_state.resize(params.width, params.height);
	int x, y, width = params.width, height = params.height;
	
	for(x = 0; x < width; x++)
		for(y = 0; y < height; y++)
			init_state[x + y*width] = hash_int_2d(params.full_x+x, params.full_y+y);

	device->mem_alloc(rng_state, MEM_READ_WRITE);
	device->mem_copy_to(rng_state);
}

bool RenderBuffers::copy_from_device()
{
	if(!buffer.device_pointer)
		return false;

	device->mem_copy_from(buffer, 0, params.width, params.height, params.get_passes_size()*sizeof(float));

	return true;
}

bool RenderBuffers::copy_to_device()
{
	if(!buffer.device_pointer)
		return false;

	device->mem_copy_to(buffer);

	return true;
}

#define LWR_FILTER_TILE 128
void RenderBuffers::filter_buffer(int sample, int half_window, float bandwidth_factor, int mode,
                                  Progress *progress,
                                  function<void(RenderTile&)> write_render_tile_cb)
{
	int nx = (params.width  + LWR_FILTER_TILE - 1) / LWR_FILTER_TILE;
	int ny = (params.height + LWR_FILTER_TILE - 1) / LWR_FILTER_TILE;
	for(int iy = 0; iy < ny; iy++) {
		for(int ix = 0; ix < nx; ix++) {
			if(progress->get_cancel())
				return;

			progress->set_status("Filtering...", string_printf("Block %d/%d", iy*nx+ix, ny*nx));

			RenderTile rtile;
			rtile.buffers = this;
			rtile.sample = sample;
			rtile.x = params.full_x + LWR_FILTER_TILE*ix;
			rtile.y = params.full_y + LWR_FILTER_TILE*iy;
			rtile.w = min(LWR_FILTER_TILE, params.width  - ix*LWR_FILTER_TILE);
			rtile.h = min(LWR_FILTER_TILE, params.height - iy*LWR_FILTER_TILE);

			DeviceTask task(DeviceTask::FILTER);
			task.x = LWR_FILTER_TILE*ix;
			task.y = LWR_FILTER_TILE*iy;
			task.w = rtile.w;
			task.h = rtile.h;
			task.offset = params.width;
			task.stride = params.height;
			task.sample = sample;
			task.buffer = buffer.device_pointer;
			task.filter_mode = mode;
			task.filter_half_window = half_window;
			task.filter_bandwidth_factor = bandwidth_factor;
			device->task_add(task);
			device->task_wait();

			write_render_tile_cb(rtile);
		}
	}
}

bool RenderBuffers::filter_lwr(int sample, int half_window, float bandwidth_factor,
                               Progress *progress,
                               function<void(RenderTile&)> write_render_tile_cb)
{
	filter_buffer(sample, half_window, bandwidth_factor, FILTER_COMBINED, progress, write_render_tile_cb);
	if(params.lwr_passes & 2)
		filter_buffer(sample, half_window, bandwidth_factor, FILTER_DIFFUSE_DIRECT, progress, write_render_tile_cb);
	if(params.lwr_passes & 4)
		filter_buffer(sample, half_window, bandwidth_factor, FILTER_DIFFUSE_INDIRECT, progress, write_render_tile_cb);
	if(params.lwr_passes & 8)
		filter_buffer(sample, half_window, bandwidth_factor, FILTER_GLOSSY_DIRECT, progress, write_render_tile_cb);
	if(params.lwr_passes & 16)
		filter_buffer(sample, half_window, bandwidth_factor, FILTER_GLOSSY_INDIRECT, progress, write_render_tile_cb);
	if(params.lwr_passes & 32)
		filter_buffer(sample, half_window, bandwidth_factor, FILTER_TRANSMISSION_DIRECT, progress, write_render_tile_cb);
	if(params.lwr_passes & 64)
		filter_buffer(sample, half_window, bandwidth_factor, FILTER_TRANSMISSION_INDIRECT, progress, write_render_tile_cb);

	return true;
}


#define FORALL_PIXELS(stride) for(int py = y; py < y+h; py++, in += pass_stride*(params.width-w)) \
                                  for(int px = x; px < x+w; px++, pixels += (stride), in += pass_stride)

bool RenderBuffers::get_pass_rect(PassType type, float exposure, int sample, int components, float *pixels, int x, int y, int w, int h)
{
	x -= params.full_x;
	y -= params.full_y;

	int pass_offset = 0;
	foreach(Pass& pass, params.passes) {
		if(pass.type != type) {
			pass_offset += pass.components;
			continue;
		}

		int pass_stride = params.get_passes_size();
		float *in = (float*)buffer.data_pointer + pass_offset + pass_stride*(params.width*y + x);

		float scale = (pass.filter)? 1.0f/(float)sample: 1.0f;
		float scale_exposure = (pass.exposure)? scale*exposure: scale;

		if(components == 1) {
			assert(pass.components == components);

			if(type == PASS_DEPTH) {
				FORALL_PIXELS(1) {
					float f = *in;
					pixels[0] = (f == 0.0f)? 1e10f: f*scale_exposure;
				}
			}
			else if(type == PASS_MIST) {
				FORALL_PIXELS(1) {
					float f = *in;
					pixels[0] = saturate(f*scale_exposure);
				}
			}
#ifdef WITH_CYCLES_DEBUG
			else if(type == PASS_RAY_BOUNCES || type == PASS_BVH_TRAVERSAL_STEPS) {
				FORALL_PIXELS(1) {
					float f = *in;
					pixels[0] = f;
				}
			}
#endif
			else {
				FORALL_PIXELS(1) {
					float f = *in;
					pixels[0] = f*scale_exposure;
				}
			}
		}
		else if(components == 3) {
			assert(pass.components == 4);

			/* RGBA */
			if(type == PASS_SHADOW) {
				FORALL_PIXELS(3) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);
					float invw = (f.w > 0.0f)? 1.0f/f.w: 1.0f;

					pixels[0] = f.x*invw;
					pixels[1] = f.y*invw;
					pixels[2] = f.z*invw;
				}
			}
			else if(pass.divide_type != PASS_NONE) {
				/* RGB lighting passes that need to divide out color */
				int divide_pass_offset = 0;
				foreach(Pass& color_pass, params.passes) {
					if(color_pass.type == pass.divide_type)
						break;
					divide_pass_offset += color_pass.components;
				}

				int divide_ofs = divide_pass_offset - pass_offset;

				FORALL_PIXELS(3) {
					float3 f = make_float3(in[0], in[1], in[2]);
					float3 f_divide = make_float3((in + divide_ofs)[0], (in + divide_ofs)[1], (in + divide_ofs)[2]);

					f = safe_divide_even_color(f*exposure, f_divide);

					pixels[0] = f.x;
					pixels[1] = f.y;
					pixels[2] = f.z;
				}
			}
			else {
				/* RGB/vector */
				FORALL_PIXELS(3) {
					float3 f = make_float3(in[0], in[1], in[2]);

					pixels[0] = f.x*scale_exposure;
					pixels[1] = f.y*scale_exposure;
					pixels[2] = f.z*scale_exposure;
				}
			}
		}
		else if(components == 4) {
			assert(pass.components == components);

			/* RGBA */
			if(type == PASS_SHADOW) {
				FORALL_PIXELS(4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);
					float invw = (f.w > 0.0f)? 1.0f/f.w: 1.0f;

					pixels[0] = f.x*invw;
					pixels[1] = f.y*invw;
					pixels[2] = f.z*invw;
					pixels[3] = 1.0f;
				}
			}
			else if(type == PASS_MOTION) {
				/* need to normalize by number of samples accumulated for motion */
				int weight_pass_offset = 0;
				foreach(Pass& color_pass, params.passes) {
					if(color_pass.type == PASS_MOTION_WEIGHT)
						break;
					weight_pass_offset += color_pass.components;
				}

				int weight_ofs = weight_pass_offset - pass_offset;

				FORALL_PIXELS(4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);
					float w = (in + weight_ofs)[0];
					float invw = (w > 0.0f)? 1.0f/w: 0.0f;

					pixels[0] = f.x*invw;
					pixels[1] = f.y*invw;
					pixels[2] = f.z*invw;
					pixels[3] = f.w*invw;
				}
			}
			else {
				FORALL_PIXELS(4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);

					pixels[0] = f.x*scale_exposure;
					pixels[1] = f.y*scale_exposure;
					pixels[2] = f.z*scale_exposure;

					/* clamp since alpha might be > 1.0 due to russian roulette */
					pixels[3] = saturate(f.w*scale);
				}
			}
		}

		return true;
	}

	return false;
}

/* Display Buffer */

DisplayBuffer::DisplayBuffer(Device *device_, bool linear)
{
	device = device_;
	draw_width = 0;
	draw_height = 0;
	transparent = true; /* todo: determine from background */
	half_float = linear;
}

DisplayBuffer::~DisplayBuffer()
{
	device_free();
}

void DisplayBuffer::device_free()
{
	if(rgba_byte.device_pointer) {
		device->pixels_free(rgba_byte);
		rgba_byte.clear();
	}
	if(rgba_half.device_pointer) {
		device->pixels_free(rgba_half);
		rgba_half.clear();
	}
}

void DisplayBuffer::reset(Device *device, BufferParams& params_)
{
	draw_width = 0;
	draw_height = 0;

	params = params_;

	/* free existing buffers */
	device_free();

	/* allocate display pixels */
	if(half_float) {
		rgba_half.resize(params.width, params.height);
		device->pixels_alloc(rgba_half);
	}
	else {
		rgba_byte.resize(params.width, params.height);
		device->pixels_alloc(rgba_byte);
	}
}

void DisplayBuffer::draw_set(int width, int height)
{
	assert(width <= params.width && height <= params.height);

	draw_width = width;
	draw_height = height;
}

void DisplayBuffer::draw(Device *device, const DeviceDrawParams& draw_params)
{
	if(draw_width != 0 && draw_height != 0) {
		device_memory& rgba = rgba_data();

		device->draw_pixels(rgba, 0, draw_width, draw_height, params.full_x, params.full_y, params.width, params.height, transparent, draw_params);
	}
}

bool DisplayBuffer::draw_ready()
{
	return (draw_width != 0 && draw_height != 0);
}

void DisplayBuffer::write(Device *device, const string& filename)
{
	int w = draw_width;
	int h = draw_height;

	if(w == 0 || h == 0)
		return;
	
	if(half_float)
		return;

	/* read buffer from device */
	device_memory& rgba = rgba_data();
	device->pixels_copy_from(rgba, 0, w, h);

	/* write image */
	ImageOutput *out = ImageOutput::create(filename);
	ImageSpec spec(w, h, 4, TypeDesc::UINT8);
	int scanlinesize = w*4*sizeof(uchar);

	out->open(filename, spec);

	/* conversion for different top/bottom convention */
	out->write_image(TypeDesc::UINT8,
		(uchar*)rgba.data_pointer + (h-1)*scanlinesize,
		AutoStride,
		-scanlinesize,
		AutoStride);

	out->close();

	delete out;
}

device_memory& DisplayBuffer::rgba_data()
{
	if(half_float)
		return rgba_half;
	else
		return rgba_byte;
}

CCL_NAMESPACE_END

