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
#include <string.h>

#include "device/device.h"
#include "device/device_intern.h"

#include "util/util_debug.h"
#include "util/util_foreach.h"
#include "util/util_half.h"
#include "util/util_math.h"
#include "util/util_opengl.h"
#include "util/util_time.h"
#include "util/util_types.h"
#include "util/util_vector.h"
#include "util/util_string.h"

CCL_NAMESPACE_BEGIN

bool Device::need_types_update = true;
bool Device::need_devices_update = true;
vector<DeviceType> Device::types;
vector<DeviceInfo> Device::devices;

/* Device Requested Features */

std::ostream& operator <<(std::ostream &os,
                          const DeviceRequestedFeatures& requested_features)
{
	os << "Experimental features: "
	   << (requested_features.experimental ? "On" : "Off") << std::endl;
	os << "Max closure count: " << requested_features.max_closure << std::endl;
	os << "Max nodes group: " << requested_features.max_nodes_group << std::endl;
	/* TODO(sergey): Decode bitflag into list of names. */
	os << "Nodes features: " << requested_features.nodes_features << std::endl;
	os << "Use Hair: "
	   << string_from_bool(requested_features.use_hair) << std::endl;
	os << "Use Object Motion: "
	   << string_from_bool(requested_features.use_object_motion) << std::endl;
	os << "Use Camera Motion: "
	   << string_from_bool(requested_features.use_camera_motion) << std::endl;
	os << "Use Baking: "
	   << string_from_bool(requested_features.use_baking) << std::endl;
	os << "Use Subsurface: "
	   << string_from_bool(requested_features.use_subsurface) << std::endl;
	os << "Use Volume: "
	   << string_from_bool(requested_features.use_volume) << std::endl;
	os << "Use Branched Integrator: "
	   << string_from_bool(requested_features.use_integrator_branched) << std::endl;
	os << "Use Patch Evaluation: "
	   << string_from_bool(requested_features.use_patch_evaluation) << std::endl;
	os << "Use Transparent Shadows: "
	   << string_from_bool(requested_features.use_transparent) << std::endl;
	os << "Use Principled BSDF: "
	   << string_from_bool(requested_features.use_principled) << std::endl;
	return os;
}

/* Device */

Device::~Device()
{
	if(!background) {
		if(vertex_buffer != 0) {
			glDeleteBuffers(1, &vertex_buffer);
		}
		if(fallback_shader_program != 0) {
			glDeleteProgram(fallback_shader_program);
		}
	}
}

void Device::pixels_alloc(device_memory& mem)
{
	mem_alloc("pixels", mem, MEM_READ_WRITE);
}

void Device::pixels_copy_from(device_memory& mem, int y, int w, int h)
{
	if(mem.data_type == TYPE_HALF)
		mem_copy_from(mem, y, w, h, sizeof(half4));
	else
		mem_copy_from(mem, y, w, h, sizeof(uchar4));
}

void Device::pixels_free(device_memory& mem)
{
	mem_free(mem);
}

const char *FALLBACK_VERTEX_SHADER =
"#version 330\n"
"uniform vec2 fullscreen;"
"in vec2 texCoord;"
"in vec2 pos;"
"out vec2 texCoord_interp;"
""
"vec2 normalize_coordinates()"
"{"
"	return (vec2(2.0) * (pos / fullscreen)) - vec2(1.0);"
"}"
""
"void main()"
"{"
"	gl_Position = vec4(normalize_coordinates(), 0.0, 1.0);"
"	texCoord_interp = texCoord;"
"}\n\0";

const char *FALLBACK_FRAGMENT_SHADER =
"#version 330\n"
"uniform sampler2D image_texture;"
"in vec2 texCoord_interp;"
"out vec4 fragColor;"
""
"float linearrgb_to_srgb(float c)"
"{"
"	if (c < 0.0031308) {"
"		return (c < 0.0) ? 0.0 : c * 12.92;"
"	}"
"	return 1.055 * pow(c, 1.0 / 2.4) - 0.055;"
"}"
""
"vec4 linearrgb_to_srgb(vec4 col_from)"
"{"
"	vec4 col_to;"
"	col_to.r = linearrgb_to_srgb(col_from.r);"
"	col_to.g = linearrgb_to_srgb(col_from.g);"
"	col_to.b = linearrgb_to_srgb(col_from.b);"
"	col_to.a = col_from.a;"
"	return col_to;"
"}"
""
"void main()"
"{"
"	fragColor = linearrgb_to_srgb(texture(image_texture, texCoord_interp));"
"}\n\0";

static int bind_fallback_shader(void)
{
	GLuint program = 0;
	program = glCreateProgram();

	struct Shader {
		const char *source;
		GLenum type;
	} shaders[2] = {
	    {FALLBACK_VERTEX_SHADER, GL_VERTEX_SHADER},
	    {FALLBACK_FRAGMENT_SHADER, GL_FRAGMENT_SHADER}
    };

	for (int i = 0; i < 2; i++) {
		GLuint shader = glCreateShader(shaders[i].type);

		std::string source_str = shaders[i].source;
		const char *c_str = source_str.c_str();

		glShaderSource(shader, 1, &c_str, NULL);
		glCompileShader(shader);

		GLint compiled;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

		if (!compiled) {
			std::cerr << "Failed to compile:" << std::endl;
			GLint log_size;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);
			char* log_msg = new char[log_size];
			glGetShaderInfoLog(shader, log_size, NULL, log_msg);
			std::cerr << log_msg << std::endl;
			delete [] log_msg;
			return 0;
		}

		glAttachShader(program, shader);
	}

	/* Link output. */
	glBindFragDataLocation(program, 0, "fragColor");

	/* Link and error check. */
	glLinkProgram(program);

	GLint linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		std::cerr << "Shader program failed to link" << std::endl;
		GLint  log_size;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_size);
		char* log_msg = new char[log_size];
		glGetProgramInfoLog(program, log_size, NULL, log_msg);
		std::cerr << log_msg << std::endl;
		delete [] log_msg;
		return 0;
	}

	return program;
}

bool Device::bind_fallback_display_space_shader(const float width, const float height)
{
	if(fallback_status == FALLBACK_SHADER_STATUS_ERROR) {
		return false;
	}

	if(fallback_status == FALLBACK_SHADER_STATUS_NONE) {
		fallback_shader_program = bind_fallback_shader();

		if (fallback_shader_program == 0) {
			fallback_status = FALLBACK_SHADER_STATUS_ERROR;
			return false;
		}

		glUseProgram(fallback_shader_program);
		image_texture_location = glGetUniformLocation(fallback_shader_program, "image_texture");
		if(image_texture_location < 0) {
			std::cerr << "Shader doesn't containt the 'image_texture' uniform." << std::endl;
			fallback_status = FALLBACK_SHADER_STATUS_ERROR;
			return false;
		}

		fullscreen_location = glGetUniformLocation(fallback_shader_program, "fullscreen");
		if(fullscreen_location < 0) {
			std::cerr << "Shader doesn't containt the 'fullscreen' uniform." << std::endl;
			fallback_status = FALLBACK_SHADER_STATUS_ERROR;
			return false;
		}
	}

	/* Run this every time. */
	glUseProgram(fallback_shader_program);
	glUniform1i(image_texture_location, 0);
	glUniform2f(fullscreen_location, width, height);
	return true;
}

void Device::draw_pixels(
    device_memory& rgba, int y,
    int w, int h, int width, int height,
    int dx, int dy, int dw, int dh,
    bool transparent, const DeviceDrawParams &draw_params)
{
	const bool use_fallback_shader = (draw_params.bind_display_space_shader_cb == NULL);
	pixels_copy_from(rgba, y, w, h);

	GLhalf *data_pointer = (GLhalf*)rgba.data_pointer;

	data_pointer += 4 * y * w;

	GLuint texid;
	glGenTextures(1, &texid);
	glBindTexture(GL_TEXTURE_2D, texid);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, w, h, 0, GL_RGBA, GL_HALF_FLOAT, data_pointer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glEnable(GL_TEXTURE_2D);

	if(transparent) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	GLint shader_program;
	if(use_fallback_shader) {
		if (!bind_fallback_display_space_shader(dw, dh)) {
			return;
		}
		shader_program = fallback_shader_program;
	}
	else {
		draw_params.bind_display_space_shader_cb();
		glGetIntegerv(GL_CURRENT_PROGRAM, &shader_program);
	}

	if(!vertex_buffer) {
		glGenBuffers(1, &vertex_buffer);
	}

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	/* invalidate old contents - avoids stalling if buffer is still waiting in queue to be rendered */
	glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);

	float *vpointer = (float *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

	if(vpointer) {
		/* texture coordinate - vertex pair */
		vpointer[0] = 0.0f;
		vpointer[1] = 0.0f;
		vpointer[2] = dx;
		vpointer[3] = dy;

		vpointer[4] = 1.0f;
		vpointer[5] = 0.0f;
		vpointer[6] = (float)width + dx;
		vpointer[7] = dy;

		vpointer[8] = 1.0f;
		vpointer[9] = 1.0f;
		vpointer[10] = (float)width + dx;
		vpointer[11] = (float)height + dy;

		vpointer[12] = 0.0f;
		vpointer[13] = 1.0f;
		vpointer[14] = dx;
		vpointer[15] = (float)height + dy;

		if(vertex_buffer) {
			glUnmapBuffer(GL_ARRAY_BUFFER);
		}
	}

	GLuint vertex_array_object;
	GLuint position_attribute, texcoord_attribute;

	glGenVertexArrays(1, &vertex_array_object);
	glBindVertexArray(vertex_array_object);

	texcoord_attribute = glGetAttribLocation(shader_program, "texCoord");
	position_attribute = glGetAttribLocation(shader_program, "pos");

	glEnableVertexAttribArray(texcoord_attribute);
	glEnableVertexAttribArray(position_attribute);

	glVertexAttribPointer(texcoord_attribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const GLvoid *)0);
	glVertexAttribPointer(position_attribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const GLvoid *)(sizeof(float) * 2));

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	if(vertex_buffer) {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if(use_fallback_shader) {
		glUseProgram(0);
	}
	else {
		draw_params.unbind_display_space_shader_cb();
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	glDeleteTextures(1, &texid);

	if(transparent) {
		glDisable(GL_BLEND);
	}
}

Device *Device::create(DeviceInfo& info, Stats &stats, bool background)
{
	Device *device;

	switch(info.type) {
		case DEVICE_CPU:
			device = device_cpu_create(info, stats, background);
			break;
#ifdef WITH_CUDA
		case DEVICE_CUDA:
			if(device_cuda_init())
				device = device_cuda_create(info, stats, background);
			else
				device = NULL;
			break;
#endif
#ifdef WITH_MULTI
		case DEVICE_MULTI:
			device = device_multi_create(info, stats, background);
			break;
#endif
#ifdef WITH_NETWORK
		case DEVICE_NETWORK:
			device = device_network_create(info, stats, "127.0.0.1");
			break;
#endif
#ifdef WITH_OPENCL
		case DEVICE_OPENCL:
			if(device_opencl_init())
				device = device_opencl_create(info, stats, background);
			else
				device = NULL;
			break;
#endif
		default:
			return NULL;
	}

	return device;
}

DeviceType Device::type_from_string(const char *name)
{
	if(strcmp(name, "CPU") == 0)
		return DEVICE_CPU;
	else if(strcmp(name, "CUDA") == 0)
		return DEVICE_CUDA;
	else if(strcmp(name, "OPENCL") == 0)
		return DEVICE_OPENCL;
	else if(strcmp(name, "NETWORK") == 0)
		return DEVICE_NETWORK;
	else if(strcmp(name, "MULTI") == 0)
		return DEVICE_MULTI;

	return DEVICE_NONE;
}

string Device::string_from_type(DeviceType type)
{
	if(type == DEVICE_CPU)
		return "CPU";
	else if(type == DEVICE_CUDA)
		return "CUDA";
	else if(type == DEVICE_OPENCL)
		return "OPENCL";
	else if(type == DEVICE_NETWORK)
		return "NETWORK";
	else if(type == DEVICE_MULTI)
		return "MULTI";

	return "";
}

vector<DeviceType>& Device::available_types()
{
	if(need_types_update) {
		types.clear();
		types.push_back(DEVICE_CPU);

#ifdef WITH_CUDA
		if(device_cuda_init())
			types.push_back(DEVICE_CUDA);
#endif

#ifdef WITH_OPENCL
		if(device_opencl_init())
			types.push_back(DEVICE_OPENCL);
#endif

#ifdef WITH_NETWORK
		types.push_back(DEVICE_NETWORK);
#endif

		need_types_update = false;
	}

	return types;
}

vector<DeviceInfo>& Device::available_devices()
{
	if(need_devices_update) {
		devices.clear();
#ifdef WITH_CUDA
		if(device_cuda_init())
			device_cuda_info(devices);
#endif

#ifdef WITH_OPENCL
		if(device_opencl_init())
			device_opencl_info(devices);
#endif

		device_cpu_info(devices);

#ifdef WITH_NETWORK
		device_network_info(devices);
#endif

		need_devices_update = false;
	}

	return devices;
}

string Device::device_capabilities()
{
	string capabilities = "CPU device capabilities: ";
	capabilities += device_cpu_capabilities() + "\n";
#ifdef WITH_CUDA
	if(device_cuda_init()) {
		capabilities += "\nCUDA device capabilities:\n";
		capabilities += device_cuda_capabilities();
	}
#endif

#ifdef WITH_OPENCL
	if(device_opencl_init()) {
		capabilities += "\nOpenCL device capabilities:\n";
		capabilities += device_opencl_capabilities();
	}
#endif

	return capabilities;
}

DeviceInfo Device::get_multi_device(vector<DeviceInfo> subdevices)
{
	assert(subdevices.size() > 1);

	DeviceInfo info;
	info.type = DEVICE_MULTI;
	info.id = "MULTI";
	info.description = "Multi Device";
	info.multi_devices = subdevices;
	info.num = 0;

	info.has_bindless_textures = true;
	info.pack_images = false;
	foreach(DeviceInfo &device, subdevices) {
		assert(device.type == info.multi_devices[0].type);

		info.pack_images |= device.pack_images;
		info.has_bindless_textures &= device.has_bindless_textures;
	}

	return info;
}

void Device::tag_update()
{
	need_types_update = true;
	need_devices_update = true;
}

void Device::free_memory()
{
	need_types_update = true;
	need_devices_update = true;
	types.free_memory();
	devices.free_memory();
}

CCL_NAMESPACE_END
