/* -------- Utils Functions --------- */

/* -------- BSDF --------- */

/* -------- Preview Lights --------- */

void node_bsdf_translucent_lights(vec4 color, vec3 N, vec3 V, vec4 ambient_light, out vec4 result)
{
	node_bsdf_diffuse_lights(color, 0.0, -N, V, ambient_light, result);
}


/* -------- Physical Lights --------- */

void bsdf_translucent_sphere_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	bsdf_diffuse_sphere_light(-N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, 0.0, ior, sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, bsdf);
}

void bsdf_translucent_area_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	bsdf_diffuse_area_light(-N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, 0.0, ior, sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, bsdf);
}

void bsdf_translucent_sun_light(
	vec3 N, vec3 T, vec3 L, vec3 V,
	vec3 l_coords, float l_distance, float l_areasizex, float l_areasizey, vec2 l_areascale, mat4 l_mat,
	float roughness, float ior, float sigma, float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out float bsdf)
{
	bsdf_diffuse_sun_light(-N, T, L, V, l_coords, l_distance, l_areasizex, l_areasizey, l_areascale, l_mat, 0.0, ior, sigma, toon_size, toon_smooth, anisotropy, aniso_rotation, bsdf);
}

/* -------- Image Based Lighting --------- */

void env_sampling_translucent(
	float pbr, vec3 viewpos, mat4 invviewmat, mat4 viewmat,
	vec3 N, vec3 T, float roughness, float ior, float sigma,
	float toon_size, float toon_smooth, float anisotropy, float aniso_rotation,
	out vec3 result)
{
	env_sampling_diffuse(
		pbr, viewpos, invviewmat, viewmat,
		-N, T, 0.0, ior, sigma,
		toon_size, toon_smooth, anisotropy, aniso_rotation,
		result);
}
