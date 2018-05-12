/* TODO use bsdf_common_lib.glsl. */
#define M_PI        3.14159265358979323846  /* pi */
#define M_2PI       6.28318530717958647692  /* 2*pi */

in vec2 texCoord_interp;
out vec4 fragColor;

uniform samplerCube images;

#define CAM_PROJ_MODE_EQUIRECTANGULAR   0
#define CAM_PROJ_MODE_FISHEYE_EQUIDIST  1
#define CAM_PROJ_MODE_FISHEYE_EQUISOLID 2

uniform float fisheye_lens;
uniform float fisheye_fov;
uniform float sensor_width;
uniform float sensor_height;
uniform vec4 equirectangular_range;
uniform int panorama_type;

float safe_acos(float x)
{
	if (x < -1.0) {
		return M_PI;
	}
	else if (x > 1.0) {
		return 0.0;
	}
	return acos(x);
}

vec3 fisheye_to_direction(in float u, in float v)
{
	u = (u - 0.5f) * 2.0f;
	v = (v - 0.5f) * 2.0f;

	float r = sqrt(u * u + v * v);

	if (r > 1.0) {
		return vec3(0.0, 0.0, 0.0);
	}

	float phi = safe_acos((r != 0.0) ? u / r : 0.0);
	float theta = r * fisheye_fov * 0.5;

	if (v < 0.0) {
		phi = -phi;
	}

	return vec3(
	        cos(phi) * sin(theta),
	        cos(theta),
	        sin(phi) * sin(theta)
	);
}

vec3 fisheye_equisolid_to_direction(in float u, in float v)
{
	u = (u - 0.5) * sensor_width;
	v = (v - 0.5) * sensor_height;

	float rmax = 2.0 * fisheye_lens * sin(fisheye_fov * 0.25);
	float r = sqrt(u * u + v * v);

	if (r > rmax) {
		return vec3(0.0, 0.0, 0.0);
	}

	float phi = safe_acos((r != 0.0) ? u / r : 0.0);
	float theta = 2.0 * asin(r / (2.0 * fisheye_lens));

	if (v < 0.0) {
		phi = -phi;
	}

	float sin_theta = sin(theta);

	return vec3(
	        cos(phi) * sin(theta),
	        cos(theta),
	        sin(phi) * sin(theta)
	);
}

vec3 equirectangular_to_direction_range(in float u, in float v, in vec4 range)
{
	float phi = range.x * u + range.y;
	float theta = range.z * v + range.w;

	return vec3(-sin(phi) * sin(theta),
	             cos(phi) * sin(theta),
	             cos(theta)
	);
}

vec3 equirectangular_to_direction(in float u, in float v)
{
	return equirectangular_to_direction_range(u, v, equirectangular_range);
}

vec3 uv_to_direction(in vec2 uv)
{
	if (panorama_type == CAM_PROJ_MODE_FISHEYE_EQUIDIST) {
		return fisheye_to_direction(uv.x, uv.y);
	}
	else if (panorama_type == CAM_PROJ_MODE_FISHEYE_EQUISOLID) {
		return fisheye_equisolid_to_direction(uv.x, uv.y);
	}
	else { /* CAM_PROJ_MODE_EQUIRECTANGULAR */
		return equirectangular_to_direction(uv.x, uv.y);
	}
}

void main()
{
	vec3 dir = uv_to_direction(texCoord_interp);
	fragColor = textureLod(images, dir, 0.0);
	fragColor.a = 1.0;
}
