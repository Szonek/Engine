// This wastes memory and can be tighly packed for better performance
// ToDo: get rid of padding areas
struct LightPacket
{
	vec4 position;      // Point and Spot: XYZ: position;          Spot: W: cutoff
	vec4 direction;    // Directional and Spot: XYZ: direction;   Spot: W: outer_cutoff
	vec4 attenuation;  // Point and Spot: XYZ: constant, linear, quadratic
	vec4 ambient;  //xyz
	vec4 diffuse;  //xyz
	vec4 specular; //xyz
};

// Stylized lighting (parameters in ViewLightingData, shadow_helpers.h). How it works: src/engine/impl/render/STYLIZED_LIGHTING.md
// With stylized_enabled == 0 every function returns exactly what the standard lighting uses.

// Faceted low-poly look: the normal of the triangle (from the screen space derivatives of the position, the same for
// the whole triangle) instead of the interpolated mesh normal. Blended by stylized_flat_shading.
vec3 stylized_shading_normal(vec3 smooth_normal, vec3 world_pos)
{
	if (stylized_enabled < 0.5f || stylized_flat_shading <= 0.0f)
	{
		return smooth_normal;
	}
	vec3 face_normal = normalize(cross(dFdx(world_pos), dFdy(world_pos)));
	// the winding seen on the screen doesn't say which side is the front, the mesh normal does
	face_normal = dot(face_normal, smooth_normal) < 0.0f ? -face_normal : face_normal;
	return normalize(mix(smooth_normal, face_normal, stylized_flat_shading));
}

// Multiplier of the ambient light: sky color on surfaces facing up, ground color on surfaces facing down.
vec3 stylized_hemisphere(vec3 normal)
{
	if (stylized_enabled < 0.5f)
	{
		return vec3(1.0f);
	}
	return mix(stylized_ground_ambient.rgb, stylized_sky_ambient.rgb, normal.y * 0.5f + 0.5f);
}

// How much of the diffuse light reaches the surface. n_dot_l: cosine of the angle between the normal and the direction to the light.
// Stylized: the light falls into stylized_bands steps of equal height, the first step starts at the terminator (n_dot_l == 0).
// 0 bands: the standard smooth falloff.
float stylized_diffuse(float n_dot_l)
{
	int bands = min(int(stylized_bands), 4);
	if (stylized_enabled < 0.5f || bands < 1)
	{
		return max(n_dot_l, 0.0f);
	}
	float half_edge = max(stylized_band_softness, 0.001f) * 0.5f;
	float t = n_dot_l * float(bands);
	float lit = 0.0f;
	for (int i = 0; i < bands; i++)
	{
		lit += smoothstep(float(i) - half_edge, float(i) + half_edge, t);
	}
	return lit / float(bands);
}

// Tinted shadows: surfaces which the light doesn't reach (facing away or in shadow) get a colored part of it instead of nothing.
// lit: how much of the light reaches the surface, 0..1
vec3 stylized_shadow_fill(float lit)
{
	if (stylized_enabled < 0.5f)
	{
		return vec3(0.0f);
	}
	return stylized_shadow_color.rgb * (1.0f - lit);
}

// Rim light: bright edge where the surface turns away from the camera. Doesn't depend on the lights.
// mesh_rim_mask: 1 the material wants the rim (skinned meshes = characters), 0 only if stylized_rim_static_meshes is set
vec3 stylized_rim(vec3 normal, vec3 view_dir, float mesh_rim_mask)
{
	if (stylized_enabled < 0.5f)
	{
		return vec3(0.0f);
	}
	float weight = max(mesh_rim_mask, stylized_rim_static_meshes);
	float rim = pow(1.0f - clamp(dot(normal, view_dir), 0.0f, 1.0f), stylized_rim_power);
	return stylized_rim_color.rgb * rim * weight;
}

// ambient_visibility: how much of the ambient light reaches the surface (ambient occlusion), 1: all of it
// direct_visibility: how much of the light coming straight from the light source reaches the surface (shadows), 1: all of it
vec3 calc_dir_light(LightPacket light, vec3 normal, vec3 view_dir, vec3 frag_color, vec3 specular_color, float shininess, float ambient_visibility, float direct_visibility)
{
	vec3 light_dir = normalize(light.direction.xyz);

	// ambient
	vec3 ambient = light.ambient.xyz * frag_color * stylized_hemisphere(normal);

	// diffuse
	float diffuse_factor = stylized_diffuse(dot(normal, light_dir));
	vec3 diffuse = light.diffuse.xyz * diffuse_factor * frag_color;

	// specular
	vec3 reflect_dir = reflect(-light_dir, normal);
	float specular_factor = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);
	vec3 specular = light.specular.xyz * specular_factor * specular_color;

	// tinted shadows, dimmed by ambient occlusion like the ambient light
	vec3 shadow_fill = light.diffuse.xyz * frag_color * stylized_shadow_fill(diffuse_factor * direct_visibility);

	return (ambient + shadow_fill) * ambient_visibility + (diffuse + specular) * direct_visibility;
}

vec3 calc_point_light(LightPacket light, vec3 normal, vec3 view_dir, vec3 frag_world_pos, vec3 frag_color, vec3 specular_color, float shininess, float ambient_visibility, float direct_visibility)
{
	vec3 light_dir = normalize(light.position.xyz - frag_world_pos); 
	float distance = length(light.position.xyz - frag_world_pos);
	float light_constant = light.attenuation.x;
	float light_linear = light.attenuation.y * distance;
	float light_quadratic = light.attenuation.z * (distance * distance);
	float attenuation = 1.0f / (light_constant + light_linear * light_quadratic);
	
	// ambient
	vec3 ambient = light.ambient.xyz * frag_color * stylized_hemisphere(normal) * attenuation;
	
	// diffuse
	float diffuse_factor = stylized_diffuse(dot(normal, light_dir));
	vec3 diffuse = light.diffuse.xyz * diffuse_factor * frag_color * attenuation;
	
	// specular
	vec3 reflect_dir = reflect(-light_dir, normal);
	float specular_factor = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);
	vec3 specular = light.specular.xyz * specular_factor * specular_color.xyz * attenuation;
	
	return ambient * ambient_visibility + (diffuse + specular) * direct_visibility;
}  

vec3 calc_spot_light(LightPacket light, vec3 normal, vec3 view_dir, vec3 frag_world_pos, vec3 frag_color, vec3 specular_color, float shininess, float ambient_visibility, float direct_visibility)
{
	vec3 light_dir = normalize(light.position.xyz - frag_world_pos); 
	float distance = length(light.position.xyz - frag_world_pos);
	float light_constant = light.attenuation.x;
	float light_linear = light.attenuation.y * distance;
	float light_quadratic = light.attenuation.z * (distance * distance);
	float attenuation = 1.0f / (light_constant + light_linear * light_quadratic);
	
	// ambient - calc first as it's not affected by cut off values
	vec3 ambient = light.ambient.xyz * frag_color * stylized_hemisphere(normal) * attenuation;
	
	float theta = dot(light_dir, normalize(-light.direction.xyz));
	float cut_off = light.position.w; // we packed cutoff into "W" component of position
	
	// calculate intensity to simulate soft, smooth edges
	float cut_off_outer = light.direction.w; // we packed outer cutoff into "W" component of direction
	float epsilon   = cut_off - cut_off_outer;
	float intensity = clamp((theta - cut_off_outer) / epsilon, 0.0, 1.0);
	
	// diffuse
	float diffuse_factor = stylized_diffuse(dot(normal, light_dir));
	vec3 diffuse = light.diffuse.xyz * diffuse_factor * frag_color * intensity * attenuation;
	
	// specular
	vec3 reflect_dir = reflect(-light_dir, normal);
	float specular_factor = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);
	vec3 specular = light.specular.xyz * specular_factor * specular_color.xyz * intensity * attenuation;
	
	return ambient * ambient_visibility + (diffuse + specular) * direct_visibility;
}  
