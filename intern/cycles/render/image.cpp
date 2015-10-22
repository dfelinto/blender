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

#include "device.h"
#include "image.h"
#include "scene.h"

#include "util_foreach.h"
#include "util_hash.h"
#include "util_image.h"
#include "util_path.h"
#include "util_progress.h"

#include <fstream>

#ifdef WITH_OSL
#include <OSL/oslexec.h>
#endif

CCL_NAMESPACE_BEGIN

ImageManager::ImageManager()
{
	need_update = true;
	pack_images = false;
	osl_texture_system = NULL;
	animation_frame = 0;

	tex_num_images = TEX_NUM_IMAGES;
	tex_num_float_images = TEX_NUM_FLOAT_IMAGES;
	tex_image_byte_start = TEX_IMAGE_BYTE_START;
}

ImageManager::~ImageManager()
{
	for(size_t slot = 0; slot < images.size(); slot++)
		assert(!images[slot]);
	for(size_t slot = 0; slot < float_images.size(); slot++)
		assert(!float_images[slot]);
}

void ImageManager::set_pack_images(bool pack_images_)
{
	pack_images = pack_images_;
}

void ImageManager::set_osl_texture_system(void *texture_system)
{
	osl_texture_system = texture_system;
}

void ImageManager::set_extended_image_limits(const DeviceInfo& info)
{
	if(info.type == DEVICE_CPU) {
		tex_num_images = TEX_EXTENDED_NUM_IMAGES_CPU;
		tex_num_float_images = TEX_EXTENDED_NUM_FLOAT_IMAGES;
		tex_image_byte_start = TEX_EXTENDED_IMAGE_BYTE_START;
	}
	else if((info.type == DEVICE_CUDA || info.type == DEVICE_MULTI) && info.extended_images) {
		tex_num_images = TEX_EXTENDED_NUM_IMAGES_GPU;
	}
	else if(info.pack_images) {
		tex_num_images = TEX_PACKED_NUM_IMAGES;
	}
}

bool ImageManager::set_animation_frame_update(int frame)
{
	if(frame != animation_frame) {
		animation_frame = frame;

		for(size_t slot = 0; slot < images.size(); slot++)
			if(images[slot] && images[slot]->animated)
				return true;

		for(size_t slot = 0; slot < float_images.size(); slot++)
			if(float_images[slot] && float_images[slot]->animated)
				return true;
	}
	
	return false;
}

bool ImageManager::is_float_image(const string& filename, void *builtin_data, bool& is_linear)
{
	bool is_float = false;
	is_linear = false;

	if(builtin_data) {
		if(builtin_image_info_cb) {
			int width, height, depth, channels;
			builtin_image_info_cb(filename, builtin_data, is_float, width, height, depth, channels);
		}

		if(is_float)
			is_linear = true;

		return is_float;
	}

	ImageInput *in = ImageInput::create(filename);

	if(in) {
		ImageSpec spec;

		if(in->open(filename, spec)) {
			/* check the main format, and channel formats;
			 * if any take up more than one byte, we'll need a float texture slot */
			if(spec.format.basesize() > 1) {
				is_float = true;
				is_linear = true;
			}

			for(size_t channel = 0; channel < spec.channelformats.size(); channel++) {
				if(spec.channelformats[channel].basesize() > 1) {
					is_float = true;
					is_linear = true;
				}
			}

			/* basic color space detection, not great but better than nothing
			 * before we do OpenColorIO integration */
			if(is_float) {
				string colorspace = spec.get_string_attribute("oiio:ColorSpace");

				is_linear = !(colorspace == "sRGB" ||
				              colorspace == "GammaCorrected" ||
				              (colorspace == "" &&
				                  (strcmp(in->format_name(), "png") == 0 ||
				                   strcmp(in->format_name(), "tiff") == 0 ||
				                   strcmp(in->format_name(), "dpx") == 0 ||
				                   strcmp(in->format_name(), "jpeg2000") == 0)));
			}
			else {
				is_linear = false;
			}

			in->close();
		}

		delete in;
	}

	return is_float;
}

static bool image_equals(ImageManager::Image *image,
                         const string& filename,
                         void *builtin_data,
                         InterpolationType interpolation,
                         ExtensionType extension)
{
	return image->filename == filename &&
	       image->builtin_data == builtin_data &&
	       image->interpolation == interpolation &&
	       image->extension == extension;
}

int ImageManager::add_image(const string& filename,
                            void *builtin_data,
                            bool animated,
                            float frame,
                            bool& is_float,
                            bool& is_linear,
                            InterpolationType interpolation,
                            ExtensionType extension,
                            bool use_alpha)
{
	Image *img;
	size_t slot;

	/* load image info and find out if we need a float texture */
	is_float = (pack_images)? false: is_float_image(filename, builtin_data, is_linear);

	if(is_float) {
		/* find existing image */
		for(slot = 0; slot < float_images.size(); slot++) {
			img = float_images[slot];
			if(img && image_equals(img,
			                       filename,
			                       builtin_data,
			                       interpolation,
			                       extension))
			{
				if(img->frame != frame) {
					img->frame = frame;
					img->need_load = true;
				}
				if(img->use_alpha != use_alpha) {
					img->use_alpha = use_alpha;
					img->need_load = true;
				}
				img->users++;
				return slot;
			}
		}

		/* find free slot */
		for(slot = 0; slot < float_images.size(); slot++) {
			if(!float_images[slot])
				break;
		}

		if(slot == float_images.size()) {
			/* max images limit reached */
			if(float_images.size() == tex_num_float_images) {
				printf("ImageManager::add_image: float image limit reached %d, skipping '%s'\n",
				       tex_num_float_images, filename.c_str());
				return -1;
			}

			float_images.resize(float_images.size() + 1);
		}

		/* add new image */
		img = new Image();
		img->filename = filename;
		img->builtin_data = builtin_data;
		img->need_load = true;
		img->animated = animated;
		img->frame = frame;
		img->interpolation = interpolation;
		img->extension = extension;
		img->users = 1;
		img->use_alpha = use_alpha;

		float_images[slot] = img;
	}
	else {
		for(slot = 0; slot < images.size(); slot++) {
			img = images[slot];
			if(img && image_equals(img,
			                       filename,
			                       builtin_data,
			                       interpolation,
			                       extension))
			{
				if(img->frame != frame) {
					img->frame = frame;
					img->need_load = true;
				}
				if(img->use_alpha != use_alpha) {
					img->use_alpha = use_alpha;
					img->need_load = true;
				}
				img->users++;
				return slot+tex_image_byte_start;
			}
		}

		/* find free slot */
		for(slot = 0; slot < images.size(); slot++) {
			if(!images[slot])
				break;
		}

		if(slot == images.size()) {
			/* max images limit reached */
			if(images.size() == tex_num_images) {
				printf("ImageManager::add_image: byte image limit reached %d, skipping '%s'\n",
				       tex_num_images, filename.c_str());
				return -1;
			}

			images.resize(images.size() + 1);
		}

		/* add new image */
		img = new Image();
		img->filename = filename;
		img->builtin_data = builtin_data;
		img->need_load = true;
		img->animated = animated;
		img->frame = frame;
		img->interpolation = interpolation;
		img->extension = extension;
		img->users = 1;
		img->use_alpha = use_alpha;

		images[slot] = img;

		slot += tex_image_byte_start;
	}

	need_update = true;

	return slot;
}

void ImageManager::remove_image(int slot)
{
	if(slot >= tex_image_byte_start) {
		slot -= tex_image_byte_start;

		assert(images[slot] != NULL);

		/* decrement user count */
		images[slot]->users--;
		assert(images[slot]->users >= 0);

		/* don't remove immediately, rather do it all together later on. one of
		 * the reasons for this is that on shader changes we add and remove nodes
		 * that use them, but we do not want to reload the image all the time. */
		if(images[slot]->users == 0)
			need_update = true;
	}
	else {
		/* decrement user count */
		float_images[slot]->users--;
		assert(float_images[slot]->users >= 0);

		/* don't remove immediately, rather do it all together later on. one of
		 * the reasons for this is that on shader changes we add and remove nodes
		 * that use them, but we do not want to reload the image all the time. */
		if(float_images[slot]->users == 0)
			need_update = true;
	}
}

void ImageManager::remove_image(const string& filename,
                                void *builtin_data,
                                InterpolationType interpolation,
                                ExtensionType extension)
{
	size_t slot;

	for(slot = 0; slot < images.size(); slot++) {
		if(images[slot] && image_equals(images[slot],
		                                filename,
		                                builtin_data,
		                                interpolation,
		                                extension))
		{
			remove_image(slot+tex_image_byte_start);
			break;
		}
	}

	if(slot == images.size()) {
		/* see if it's in a float texture slot */
		for(slot = 0; slot < float_images.size(); slot++) {
			if(float_images[slot] && image_equals(float_images[slot],
			                                      filename,
			                                      builtin_data,
			                                      interpolation,
			                                      extension)) {
				remove_image(slot);
				break;
			}
		}
	}
}

/* TODO(sergey): Deduplicate with the iteration above, but make it pretty,
 * without bunch of arguments passing around making code readability even
 * more cluttered.
 */
void ImageManager::tag_reload_image(const string& filename,
                                    void *builtin_data,
                                    InterpolationType interpolation,
                                    ExtensionType extension)
{
	size_t slot;

	for(slot = 0; slot < images.size(); slot++) {
		if(images[slot] && image_equals(images[slot],
		                                filename,
		                                builtin_data,
		                                interpolation,
		                                extension)) {
			images[slot]->need_load = true;
			break;
		}
	}

	if(slot == images.size()) {
		/* see if it's in a float texture slot */
		for(slot = 0; slot < float_images.size(); slot++) {
			if(float_images[slot] && image_equals(float_images[slot],
			                                      filename,
			                                      builtin_data,
			                                      interpolation,
			                                      extension)) {
				float_images[slot]->need_load = true;
				break;
			}
		}
	}
}

ImageManager::IESLight::IESLight(const string& ies_)
{
	ies = ies_;
	users = 1;
	hash = hash_string(ies.c_str());

	if(!parse() || !process()) {
		for(int i = 0; i < intensity.size(); i++)
			delete[] intensity[i];
		intensity.clear();
		v_angles_num = h_angles_num = 0;
	}
}

ImageManager::IESLight::IESLight()
{
	v_angles_num = h_angles_num = 0;
}

bool ImageManager::IESLight::parse()
{
	int len = ies.length();
	char *fdata = new char[len+1];
	memcpy(fdata, ies.c_str(), len+1);

	for(int i = 0; i < len; i++)
		if(fdata[i] == ',')
			fdata[i] = ' ';

	char *data = strstr(fdata, "\nTILT=");
	if(!data) {
		delete[] fdata;
		return false;
	}

	if(strncmp(data, "\nTILT=INCLUDE", 13) == 0)
		for(int i = 0; i < 5 && data; i++)
			data = strstr(data+1, "\n");
	else
		data = strstr(data+1, "\n");
	if(!data) {
		delete[] fdata;
		return false;
	}

	data++;
	strtol(data, &data, 10); /* Number of lamps */
	strtod(data, &data); /* Lumens per lamp */
	double factor = strtod(data, &data); /* Candela multiplier */
	v_angles_num = strtol(data, &data, 10); /* Number of vertical angles */
	h_angles_num = strtol(data, &data, 10); /* Number of horizontal angles */
	strtol(data, &data, 10); /* Photometric type (is assumed to be 1 => Type C) */
	strtol(data, &data, 10); /* Unit of the geometry data */
	strtod(data, &data); /* Width */
	strtod(data, &data); /* Length */
	strtod(data, &data); /* Height */
	factor *= strtod(data, &data); /* Ballast factor */
	factor *= strtod(data, &data); /* Ballast-Lamp Photometric factor */
	strtod(data, &data); /* Input Watts */

	/* Intensity values in IES files are specified in candela (lumen/sr), a photometric quantity.
	 * Cycles expects radiometric quantities, though, which requires a conversion.
	 * However, the Luminous efficacy (ratio of lumens per Watt) depends on the spectral distribution
	 * of the light source since lumens take human perception into account.
	 * Since this spectral distribution is not known from the IES file, a typical one must be assumed.
	 * The D65 standard illuminant has a Luminous efficacy of 177.83, which is used here to convert to Watt/sr.
	 * A more advanced approach would be to add a Blackbody Temperature input to the node and numerically
	 * integrate the Luminous efficacy from the resulting spectral distribution.
	 * Also, the Watt/sr value must be multiplied by 4*pi to get the Watt value that Cycles expects
	 * for lamp strength. Therefore, the conversion here uses 4*pi/177.83 as a Candela to Watt factor.
	 */
	factor *= 0.0706650768394;

	for(int i = 0; i < v_angles_num; i++)
		v_angles.push_back(strtod(data, &data));
	for(int i = 0; i < h_angles_num; i++)
		h_angles.push_back(strtod(data, &data));
	for(int i = 0; i < h_angles_num; i++) {
		intensity.push_back(new float[v_angles_num]);
	for(int j = 0; j < v_angles_num; j++)
			intensity[i][j] = factor * strtod(data, &data);
	}
	for(; isspace(*data); data++);
	if(*data == 0 || strncmp(data, "END", 3) == 0) {
		delete[] fdata;
		return true;
	}
	delete[] fdata;
	return false;
}

bool ImageManager::IESLight::process()
{
	if(h_angles_num == 0 || v_angles_num == 0 || h_angles[0] != 0.0f || v_angles[0] != 0.0f)
		return false;

	if(h_angles_num == 1) {
		/* 1D IES */
		h_angles_num = 2;
		h_angles.push_back(360.f);
		intensity.push_back(new float[v_angles_num]);
		memcpy(intensity[1], intensity[0], v_angles_num*sizeof(float));
	}
	else {
		if(!(h_angles[h_angles_num-1] == 90.0f || h_angles[h_angles_num-1] == 180.0f || h_angles[h_angles_num-1] == 360.0f))
			return false;
		/* 2D IES - potential symmetries must be considered here */
		if(h_angles[h_angles_num-1] == 90.0f) {
			/* All 4 quadrants are symmetric */
			for(int i = h_angles_num-2; i >= 0; i--) {
				intensity.push_back(new float[v_angles_num]);
				memcpy(intensity[intensity.size()-1], intensity[i], v_angles_num*sizeof(float));
				h_angles.push_back(180.0f - h_angles[i]);
			}
			h_angles_num = 2*h_angles_num-1;
		}
		if(h_angles[h_angles_num-1] == 180.0f) {
			/* Quadrants 1 and 2 are symmetric with 3 and 4 */
			for(int i = h_angles_num-2; i >= 0; i--) {
				intensity.push_back(new float[v_angles_num]);
				memcpy(intensity[intensity.size()-1], intensity[i], v_angles_num*sizeof(float));
				h_angles.push_back(360.0f - h_angles[i]);
			}
			h_angles_num = 2*h_angles_num-1;
		}
	}

	return true;
}

ImageManager::IESLight::~IESLight()
{
	for(int i = 0; i < intensity.size(); i++)
		delete[] intensity[i];
}

int ImageManager::add_ies_from_file(const string& filename)
{
	string content;
	std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
	if(in)
		content = string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	/* If the file can't be opened, call with an empty string */
	return add_ies(content);
}

int ImageManager::add_ies(const string& content)
{
	uint hash = hash_string(content.c_str());

	size_t slot;
	IESLight *ies;
	for(slot = 0; slot < ies_lights.size(); slot++) {
		ies = ies_lights[slot];
		if(ies && ies->hash == hash) {
			ies->users++;
			return slot;
		}
	}
	for(slot = 0; slot < ies_lights.size(); slot++) {
		if(!ies_lights[slot])
			break;
	}

	if(slot == ies_lights.size())
		ies_lights.resize(ies_lights.size() + 1);

	ies = new IESLight(content);

	ies_lights[slot] = ies;
	need_update = true;

	return slot;
}

void ImageManager::remove_ies(int slot)
{
	if(slot < 0 || slot >= ies_lights.size())
		return;

	ies_lights[slot]->users--;
	assert(ies_lights[slot]->users >= 0);

	if(ies_lights[slot]->users == 0)
		need_update = true;
}

bool ImageManager::file_load_image(Image *img, device_vector<uchar4>& tex_img)
{
	if(img->filename == "")
		return false;

	ImageInput *in = NULL;
	int width, height, depth, components;

	if(!img->builtin_data) {
		/* load image from file through OIIO */
		in = ImageInput::create(img->filename);

		if(!in)
			return false;

		ImageSpec spec = ImageSpec();
		ImageSpec config = ImageSpec();

		if(img->use_alpha == false)
			config.attribute("oiio:UnassociatedAlpha", 1);

		if(!in->open(img->filename, spec, config)) {
			delete in;
			return false;
		}

		width = spec.width;
		height = spec.height;
		depth = spec.depth;
		components = spec.nchannels;
	}
	else {
		/* load image using builtin images callbacks */
		if(!builtin_image_info_cb || !builtin_image_pixels_cb)
			return false;

		bool is_float;
		builtin_image_info_cb(img->filename, img->builtin_data, is_float, width, height, depth, components);
	}

	/* we only handle certain number of components */
	if(!(components >= 1 && components <= 4)) {
		if(in) {
			in->close();
			delete in;
		}

		return false;
	}

	/* read RGBA pixels */
	uchar *pixels = (uchar*)tex_img.resize(width, height, depth);
	if(pixels == NULL) {
		return false;
	}
	bool cmyk = false;

	if(in) {
		if(depth <= 1) {
			int scanlinesize = width*components*sizeof(uchar);

			in->read_image(TypeDesc::UINT8,
				(uchar*)pixels + (((size_t)height)-1)*scanlinesize,
				AutoStride,
				-scanlinesize,
				AutoStride);
		}
		else {
			in->read_image(TypeDesc::UINT8, (uchar*)pixels);
		}

		cmyk = strcmp(in->format_name(), "jpeg") == 0 && components == 4;

		in->close();
		delete in;
	}
	else {
		builtin_image_pixels_cb(img->filename, img->builtin_data, pixels);
	}

	size_t num_pixels = ((size_t)width) * height * depth;
	if(cmyk) {
		/* CMYK */
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+2] = (pixels[i*4+2]*pixels[i*4+3])/255;
			pixels[i*4+1] = (pixels[i*4+1]*pixels[i*4+3])/255;
			pixels[i*4+0] = (pixels[i*4+0]*pixels[i*4+3])/255;
			pixels[i*4+3] = 255;
		}
	}
	else if(components == 2) {
		/* grayscale + alpha */
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+3] = pixels[i*2+1];
			pixels[i*4+2] = pixels[i*2+0];
			pixels[i*4+1] = pixels[i*2+0];
			pixels[i*4+0] = pixels[i*2+0];
		}
	}
	else if(components == 3) {
		/* RGB */
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+3] = 255;
			pixels[i*4+2] = pixels[i*3+2];
			pixels[i*4+1] = pixels[i*3+1];
			pixels[i*4+0] = pixels[i*3+0];
		}
	}
	else if(components == 1) {
		/* grayscale */
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+3] = 255;
			pixels[i*4+2] = pixels[i];
			pixels[i*4+1] = pixels[i];
			pixels[i*4+0] = pixels[i];
		}
	}

	if(img->use_alpha == false) {
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+3] = 255;
		}
	}

	return true;
}

bool ImageManager::file_load_float_image(Image *img, device_vector<float4>& tex_img)
{
	if(img->filename == "")
		return false;

	ImageInput *in = NULL;
	int width, height, depth, components;

	if(!img->builtin_data) {
		/* load image from file through OIIO */
		in = ImageInput::create(img->filename);

		if(!in)
			return false;

		ImageSpec spec = ImageSpec();
		ImageSpec config = ImageSpec();

		if(img->use_alpha == false)
			config.attribute("oiio:UnassociatedAlpha",1);

		if(!in->open(img->filename, spec, config)) {
			delete in;
			return false;
		}

		/* we only handle certain number of components */
		width = spec.width;
		height = spec.height;
		depth = spec.depth;
		components = spec.nchannels;
	}
	else {
		/* load image using builtin images callbacks */
		if(!builtin_image_info_cb || !builtin_image_float_pixels_cb)
			return false;

		bool is_float;
		builtin_image_info_cb(img->filename, img->builtin_data, is_float, width, height, depth, components);
	}

	if(components < 1 || width == 0 || height == 0) {
		if(in) {
			in->close();
			delete in;
		}
		return false;
	}

	/* read RGBA pixels */
	float *pixels = (float*)tex_img.resize(width, height, depth);
	if(pixels == NULL) {
		return false;
	}
	bool cmyk = false;

	if(in) {
		float *readpixels = pixels;
		vector<float> tmppixels;

		if(components > 4) {
			tmppixels.resize(((size_t)width)*height*components);
			readpixels = &tmppixels[0];
		}

		if(depth <= 1) {
			int scanlinesize = width*components*sizeof(float);

			in->read_image(TypeDesc::FLOAT,
				(uchar*)readpixels + (height-1)*scanlinesize,
				AutoStride,
				-scanlinesize,
				AutoStride);
		}
		else {
			in->read_image(TypeDesc::FLOAT, (uchar*)readpixels);
		}

		if(components > 4) {
			size_t dimensions = ((size_t)width)*height;
			for(size_t i = dimensions-1, pixel = 0; pixel < dimensions; pixel++, i--) {
				pixels[i*4+3] = tmppixels[i*components+3];
				pixels[i*4+2] = tmppixels[i*components+2];
				pixels[i*4+1] = tmppixels[i*components+1];
				pixels[i*4+0] = tmppixels[i*components+0];
			}

			tmppixels.clear();
		}

		cmyk = strcmp(in->format_name(), "jpeg") == 0 && components == 4;

		in->close();
		delete in;
	}
	else {
		builtin_image_float_pixels_cb(img->filename, img->builtin_data, pixels);
	}

	size_t num_pixels = ((size_t)width) * height * depth;
	if(cmyk) {
		/* CMYK */
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+3] = 255;
			pixels[i*4+2] = (pixels[i*4+2]*pixels[i*4+3])/255;
			pixels[i*4+1] = (pixels[i*4+1]*pixels[i*4+3])/255;
			pixels[i*4+0] = (pixels[i*4+0]*pixels[i*4+3])/255;
		}
	}
	else if(components == 2) {
		/* grayscale + alpha */
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+3] = pixels[i*2+1];
			pixels[i*4+2] = pixels[i*2+0];
			pixels[i*4+1] = pixels[i*2+0];
			pixels[i*4+0] = pixels[i*2+0];
		}
	}
	else if(components == 3) {
		/* RGB */
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+3] = 1.0f;
			pixels[i*4+2] = pixels[i*3+2];
			pixels[i*4+1] = pixels[i*3+1];
			pixels[i*4+0] = pixels[i*3+0];
		}
	}
	else if(components == 1) {
		/* grayscale */
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+3] = 1.0f;
			pixels[i*4+2] = pixels[i];
			pixels[i*4+1] = pixels[i];
			pixels[i*4+0] = pixels[i];
		}
	}

	if(img->use_alpha == false) {
		for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
			pixels[i*4+3] = 1.0f;
		}
	}

	return true;
}

void ImageManager::device_load_image(Device *device, DeviceScene *dscene, int slot, Progress *progress)
{
	if(progress->get_cancel())
		return;
	
	Image *img;
	bool is_float;

	if(slot >= tex_image_byte_start) {
		img = images[slot - tex_image_byte_start];
		is_float = false;
	}
	else {
		img = float_images[slot];
		is_float = true;
	}

	if(osl_texture_system && !img->builtin_data)
		return;

	if(is_float) {
		string filename = path_filename(float_images[slot]->filename);
		progress->set_status("Updating Images", "Loading " + filename);

		device_vector<float4>& tex_img = dscene->tex_float_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_float_image(img, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			float *pixels = (float*)tex_img.resize(1, 1);

			pixels[0] = TEX_IMAGE_MISSING_R;
			pixels[1] = TEX_IMAGE_MISSING_G;
			pixels[2] = TEX_IMAGE_MISSING_B;
			pixels[3] = TEX_IMAGE_MISSING_A;
		}

		string name;

		if(slot >= 100) name = string_printf("__tex_image_float_%d", slot);
		else if(slot >= 10) name = string_printf("__tex_image_float_0%d", slot);
		else name = string_printf("__tex_image_float_00%d", slot);

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(),
			                  tex_img,
			                  img->interpolation,
			                  img->extension);
		}
	}
	else {
		string filename = path_filename(images[slot - tex_image_byte_start]->filename);
		progress->set_status("Updating Images", "Loading " + filename);

		device_vector<uchar4>& tex_img = dscene->tex_image[slot - tex_image_byte_start];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_image(img, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			uchar *pixels = (uchar*)tex_img.resize(1, 1);

			pixels[0] = (TEX_IMAGE_MISSING_R * 255);
			pixels[1] = (TEX_IMAGE_MISSING_G * 255);
			pixels[2] = (TEX_IMAGE_MISSING_B * 255);
			pixels[3] = (TEX_IMAGE_MISSING_A * 255);
		}

		string name;

		if(slot >= 100) name = string_printf("__tex_image_%d", slot);
		else if(slot >= 10) name = string_printf("__tex_image_0%d", slot);
		else name = string_printf("__tex_image_00%d", slot);

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(),
			                  tex_img,
			                  img->interpolation,
			                  img->extension);
		}
	}

	img->need_load = false;
}

void ImageManager::device_free_image(Device *device, DeviceScene *dscene, int slot)
{
	Image *img;
	bool is_float;

	if(slot >= tex_image_byte_start) {
		img = images[slot - tex_image_byte_start];
		is_float = false;
	}
	else {
		img = float_images[slot];
		is_float = true;
	}

	if(img) {
		if(osl_texture_system && !img->builtin_data) {
#ifdef WITH_OSL
			ustring filename(images[slot]->filename);
			((OSL::TextureSystem*)osl_texture_system)->invalidate(filename);
#endif
		}
		else if(is_float) {
			device_vector<float4>& tex_img = dscene->tex_float_image[slot];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();

			delete float_images[slot];
			float_images[slot] = NULL;
		}
		else {
			device_vector<uchar4>& tex_img = dscene->tex_image[slot - tex_image_byte_start];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();

			delete images[slot - tex_image_byte_start];
			images[slot - tex_image_byte_start] = NULL;
		}
	}
}

void ImageManager::device_update(Device *device, DeviceScene *dscene, Progress& progress)
{
	if(!need_update)
		return;

	TaskPool pool;

	for(size_t slot = 0; slot < images.size(); slot++) {
		if(!images[slot])
			continue;

		if(images[slot]->users == 0) {
			device_free_image(device, dscene, slot + tex_image_byte_start);
		}
		else if(images[slot]->need_load) {
			if(!osl_texture_system || images[slot]->builtin_data) 
				pool.push(function_bind(&ImageManager::device_load_image, this, device, dscene, slot + tex_image_byte_start, &progress));
		}
	}

	for(size_t slot = 0; slot < float_images.size(); slot++) {
		if(!float_images[slot])
			continue;

		if(float_images[slot]->users == 0) {
			device_free_image(device, dscene, slot);
		}
		else if(float_images[slot]->need_load) {
			if(!osl_texture_system || float_images[slot]->builtin_data) 
				pool.push(function_bind(&ImageManager::device_load_image, this, device, dscene, slot, &progress));
		}
	}

	for(size_t slot = 0; slot < ies_lights.size(); slot++) {
		if(!ies_lights[slot])
			continue;

		if(ies_lights[slot]->users == 0) {
			delete ies_lights[slot];
			ies_lights[slot] = NULL;
		}
	}

	pool.wait_work();

	if(pack_images)
		device_pack_images(device, dscene, progress);
	device_update_ies(device, dscene);

	need_update = false;
}

void ImageManager::device_update_ies(Device *device,
                                     DeviceScene *dscene)
{
	KernelIntegrator *kintegrator = &dscene->data.integrator;
	if(ies_lights.size() > 0) {
		int max_data_len = 0;
		for(int i = 0; i < ies_lights.size(); i++) {
			IESLight *ies = ies_lights[i];
			int data_len;
			if(ies && ies->v_angles_num > 0 && ies->h_angles_num > 0)
				data_len = 2 + ies->h_angles_num + ies->v_angles_num + ies->h_angles_num*ies->v_angles_num;
			else data_len = 10;
			max_data_len = max(max_data_len, data_len);
		}

		int len = max_data_len*ies_lights.size();
		float *data = dscene->ies_lights.resize(len);
		for(int i = 0; i < ies_lights.size(); i++) {
			float *ies_data = data + max_data_len*i;
			IESLight *ies = ies_lights[i];
			if(ies && ies->v_angles_num > 0 && ies->h_angles_num > 0) {
				*(ies_data++) = __int_as_float(ies->h_angles_num);
				*(ies_data++) = __int_as_float(ies->v_angles_num);
				for(int h = 0; h < ies->h_angles_num; h++)
					*(ies_data++) = ies->h_angles[h] / 180.f * M_PI_F;
				for(int v = 0; v < ies->v_angles_num; v++)
					*(ies_data++) = ies->v_angles[v] / 180.f * M_PI_F;
				for(int h = 0; h < ies->h_angles_num; h++)
					for(int v = 0; v < ies->v_angles_num; v++)
						 *(ies_data++) = ies->intensity[h][v];
			}
			else {
				/* IES was not loaded correctly => Fallback */
				*(ies_data++) = __int_as_float(2);
				*(ies_data++) = __int_as_float(2);
				*(ies_data++) = 0.0f;
				*(ies_data++) = M_2PI_F;
				*(ies_data++) = 0.0f;
				*(ies_data++) = M_PI_2_F;
				*(ies_data++) = 100.0f;
				*(ies_data++) = 100.0f;
				*(ies_data++) = 100.0f;
				*(ies_data++) = 100.0f;
			}
		}

		if(dscene->ies_lights.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(dscene->ies_lights);
		}
		device->tex_alloc("__ies", dscene->ies_lights);

		kintegrator->ies_stride = max_data_len;
	}
	else kintegrator->ies_stride = 0;
}

void ImageManager::device_update_slot(Device *device,
                                      DeviceScene *dscene,
                                      int slot,
                                      Progress *progress)
{
	Image *image;
	if(slot >= tex_image_byte_start) {
		int byte_slot = slot - tex_image_byte_start;
		assert(images[byte_slot] != NULL);
		image = images[byte_slot];
	}
	else {
		assert(float_images[slot] != NULL);
		image = float_images[slot];
	}
	if(image->users == 0) {
		device_free_image(device, dscene, slot);
	}
	else if(image->need_load) {
		if(!osl_texture_system || float_images[slot]->builtin_data)
			device_load_image(device,
			                  dscene,
			                  slot,
			                  progress);
	}
}

void ImageManager::device_pack_images(Device *device,
                                      DeviceScene *dscene,
                                      Progress& /*progess*/)
{
	/* for OpenCL, we pack all image textures inside a single big texture, and
	 * will do our own interpolation in the kernel */
	size_t size = 0;

	for(size_t slot = 0; slot < images.size(); slot++) {
		if(!images[slot])
			continue;

		device_vector<uchar4>& tex_img = dscene->tex_image[slot];
		size += tex_img.size();
	}

	uint4 *info = dscene->tex_image_packed_info.resize(images.size());
	uchar4 *pixels = dscene->tex_image_packed.resize(size);

	size_t offset = 0;

	for(size_t slot = 0; slot < images.size(); slot++) {
		if(!images[slot])
			continue;

		device_vector<uchar4>& tex_img = dscene->tex_image[slot];

		/* todo: support 3D textures, only CPU for now */

		/* The image options are packed
		   bit 0 -> periodic
		   bit 1 + 2 -> interpolation type */
		uint8_t interpolation = (images[slot]->interpolation << 1) + 1;
		info[slot] = make_uint4(tex_img.data_width, tex_img.data_height, offset, interpolation);

		memcpy(pixels+offset, (void*)tex_img.data_pointer, tex_img.memory_size());
		offset += tex_img.size();
	}

	if(dscene->tex_image_packed.size()) {
		if(dscene->tex_image_packed.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(dscene->tex_image_packed);
		}
		device->tex_alloc("__tex_image_packed", dscene->tex_image_packed);
	}
	if(dscene->tex_image_packed_info.size()) {
		if(dscene->tex_image_packed_info.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(dscene->tex_image_packed_info);
		}
		device->tex_alloc("__tex_image_packed_info", dscene->tex_image_packed_info);
	}
}

void ImageManager::device_free_builtin(Device *device, DeviceScene *dscene)
{
	for(size_t slot = 0; slot < images.size(); slot++)
		if(images[slot] && images[slot]->builtin_data)
			device_free_image(device, dscene, slot + tex_image_byte_start);

	for(size_t slot = 0; slot < float_images.size(); slot++)
		if(float_images[slot] && float_images[slot]->builtin_data)
			device_free_image(device, dscene, slot);
}

void ImageManager::device_free(Device *device, DeviceScene *dscene)
{
	for(size_t slot = 0; slot < images.size(); slot++)
		device_free_image(device, dscene, slot + tex_image_byte_start);
	for(size_t slot = 0; slot < float_images.size(); slot++)
		device_free_image(device, dscene, slot);

	device->tex_free(dscene->tex_image_packed);
	device->tex_free(dscene->tex_image_packed_info);
	device->tex_free(dscene->ies_lights);

	dscene->tex_image_packed.clear();
	dscene->tex_image_packed_info.clear();
	dscene->ies_lights.clear();

	images.clear();
	float_images.clear();
	ies_lights.clear();
}

CCL_NAMESPACE_END

