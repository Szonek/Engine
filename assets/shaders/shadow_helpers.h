// Shadows and ambient occlusion of the lit shaders. How it works: src/engine/impl/render/SHADOWS_AND_SSAO.md

#define DEBUG_VIEW_AMBIENT_OCCLUSION 1u
#define DEBUG_VIEW_SHADOW 2u

// has to match ViewLightingGpuData (render_scene_data.h)
layout (binding = ENGINE_BINDING_SLOT_VIEW_LIGHTING_DATA, std140) uniform ViewLightingData
{
	mat4 shadow_view_projection;  // world space -> clip space of the shadow map
	float shadow_enabled;
	float shadow_depth_bias;
	float shadow_normal_offset;   // world units
	float shadow_strength;
	float shadow_fade_start;      // distance from the camera
	float shadow_fade_end;
	int shadow_pcf_radius;
	float ssao_enabled;
	float ssao_direct_light_occlusion;
	uint debug_view;
	float view_lighting_pad0_;
	float view_lighting_pad1_;
	// stylized lighting, used by lit_helpers.h. How it works: src/engine/impl/render/STYLIZED_LIGHTING.md
	vec4 stylized_sky_ambient;        // rgb: ambient light multiplier of surfaces facing up
	vec4 stylized_ground_ambient;     // rgb: ambient light multiplier of surfaces facing down
	vec4 stylized_shadow_color;       // rgb: part of the directional light that unlit surfaces still get
	vec4 stylized_rim_color;          // rgb: rim light color multiplied by its strength
	float stylized_enabled;           // 1: stylized lighting, 0: standard lighting (all stylized_* values are ignored)
	float stylized_bands;             // number of steps of the diffuse light, 0: smooth (standard) falloff
	float stylized_band_softness;     // 0: hard step edges, 1: steps blend into each other
	float stylized_rim_power;         // higher: thinner rim
	float stylized_rim_static_meshes; // 1: rim on all meshes, 0: only on meshes whose material asks for it (rim_mask)
	float stylized_flat_shading;      // 0: smooth normals of the mesh, 1: one normal per triangle (faceted low-poly look)
	float view_lighting_pad3_;
	float view_lighting_pad4_;
};

// depth of the scene seen from the directional light
layout(binding = ENGINE_BINDING_SLOT_TEXTURE_SHADOW_MAP) uniform sampler2D texture_shadow_map;
// screen space ambient occlusion, same size as the screen. 1: not occluded
layout(binding = ENGINE_BINDING_SLOT_TEXTURE_SSAO) uniform sampler2D texture_ssao;

// 1: receiver is closer to the light than what the shadow map stores at given texel (lit), 0: something is in front of it (shadow)
float shadow_map_test(ivec2 texel, ivec2 map_size, float receiver_depth)
{
	float occluder_depth = texelFetch(texture_shadow_map, clamp(texel, ivec2(0), map_size - 1), 0).r;
	return receiver_depth <= occluder_depth ? 1.0f : 0.0f;
}

// How much light of the directional light reaches the surface. 1: all of it, 0: surface is in shadow.
float calc_directional_shadow(vec3 world_pos, vec3 normal, vec3 dir_to_light, float distance_to_camera)
{
	if (shadow_enabled < 0.5f)
	{
		return 1.0f;
	}

	// Normal offset bias: the lookup is done for a point moved away from the surface. The steeper the light hits the surface,
	// the more depth changes within one shadow map texel, so the bigger offset is needed.
	float n_dot_l = clamp(dot(normal, dir_to_light), 0.0f, 1.0f);
	vec3 biased_pos = world_pos + normal * (shadow_normal_offset * sqrt(1.0f - n_dot_l * n_dot_l));

	// position in the shadow map: xy - where to look, z - depth of this fragment as the light sees it.
	// Projection of the light is orthographic, so there is no perspective divide.
	vec3 shadow_coords = (shadow_view_projection * vec4(biased_pos, 1.0f)).xyz * 0.5f + 0.5f;
	if (any(lessThan(shadow_coords, vec3(0.0f))) || any(greaterThan(shadow_coords, vec3(1.0f))))
	{
		return 1.0f; // outside of the shadow map
	}
	float receiver_depth = shadow_coords.z - shadow_depth_bias;
	ivec2 map_size = textureSize(texture_shadow_map, 0);

	float lit = 0.0f;
	if (shadow_pcf_radius <= 0)
	{
		// one test: hard, pixelated edge
		lit = shadow_map_test(ivec2(shadow_coords.xy * vec2(map_size)), map_size, receiver_depth);
	}
	else
	{
		// Percentage closer filtering: results of many tests are averaged (depths themselves can't be averaged).
		// It's a box of (2r+1) x (2r+1) texels centered exactly at the lookup position: the texels on its border
		// are covered only partially, so they are weighted by the covered fraction. That makes the edge move smoothly.
		vec2 texel_pos = shadow_coords.xy * vec2(map_size) - 0.5f;
		ivec2 base_texel = ivec2(floor(texel_pos));
		vec2 fraction = fract(texel_pos);
		int r = shadow_pcf_radius;
		for (int y = -r; y <= r + 1; y++)
		{
			float weight_y = (y == -r) ? (1.0f - fraction.y) : ((y == r + 1) ? fraction.y : 1.0f);
			for (int x = -r; x <= r + 1; x++)
			{
				float weight_x = (x == -r) ? (1.0f - fraction.x) : ((x == r + 1) ? fraction.x : 1.0f);
				lit += weight_x * weight_y * shadow_map_test(base_texel + ivec2(x, y), map_size, receiver_depth);
			}
		}
		float box_size = float(2 * r + 1);
		lit /= box_size * box_size;
	}

	// the shadow map covers only the surroundings of the camera, fade the shadow out before it ends
	lit = mix(lit, 1.0f, smoothstep(shadow_fade_start, shadow_fade_end, distance_to_camera));
	return mix(1.0f, lit, shadow_strength);
}

// How much ambient light reaches the surface. 1: all of it.
float sample_ambient_occlusion()
{
	if (ssao_enabled < 0.5f)
	{
		return 1.0f;
	}
	return texelFetch(texture_ssao, ivec2(gl_FragCoord.xy), 0).r;
}
