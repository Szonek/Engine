// Particles: colour times texture (or a soft circle), eroded, lit like a ball of smoke and faded into the geometry
// behind them, written for the batch's blend mode (ParticleBlend): 0 alpha (src alpha, 1 - src alpha),
// 1 premultiplied (one, 1 - src alpha; the texture is premultiplied), 2 additive (one, one).
// Needs shadow_helpers.h (and lit_helpers.h for LightPacket) before it.
in PARTICLE_OUT
{
	vec2 uv;
	vec2 frame_uv;
	vec2 next_frame_uv;
	vec2 sphere;
	vec4 color;
	vec3 world_position;
	float view_depth;
	float erosion;
	float frame_blend;
} fs_in;

uniform mat4 view;
uniform mat4 inverse_projection;
uniform uint blend_mode;
uniform uint has_texture;
layout (binding = ENGINE_BINDING_SLOT_TEXTURE_PARTICLE) uniform sampler2D particle_texture;
// lit by the scene's directional lights
uniform uint lit;
// soft: faded out over this distance (world units) in front of the opaque geometry; 0 is off
uniform float soft_distance;
// the opaque scene's depth (window space, 0..1), sampled only when soft_distance > 0
layout (binding = ENGINE_BINDING_SLOT_TEXTURE_SCENE_DEPTH) uniform sampler2D scene_depth;

layout (binding = ENGINE_BINDING_SLOT_SCENE_DATA, std140) uniform SceneData
{
	uint direction_light_count;
	uint point_light_count;
	uint spot_light_count;
	float pad0_;
};
layout (binding = ENGINE_BINDING_SLOT_LIGHTS_DATA, std430) readonly buffer LightPacketSSBO
{
	LightPacket light_data[];
};

layout (location = 0) out vec4 out_fragment_color;

// how much light reaches a side facing away from it: smoke scatters light through itself, so its shadow side isn't black
const float WRAP = 0.5;

// a quad lit as the ball of smoke it shows: the normal of a sphere in front of it, wrapped diffuse light of the
// directional lights, the first one's shadow taken where the sphere's surface is
vec3 light_particle(vec3 albedo)
{
	vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 up = vec3(view[0][1], view[1][1], view[2][1]);
	vec3 to_camera = vec3(view[0][2], view[1][2], view[2][2]);
	float bulge = sqrt(max(0.0, 1.0 - dot(fs_in.sphere, fs_in.sphere)));
	vec3 normal = normalize(right * fs_in.sphere.x + up * fs_in.sphere.y + to_camera * bulge);

	vec3 color = vec3(0.0);
	for (uint i = 0u; i < direction_light_count; i++)
	{
		vec3 light_dir = normalize(light_data[i].direction.xyz);
		float diffuse = max((dot(normal, light_dir) + WRAP) / (1.0 + WRAP), 0.0);
		float shadow = 1.0;
		if (i == 0u)
		{
			shadow = calc_directional_shadow(fs_in.world_position, normal, light_dir, fs_in.view_depth);
		}
		color += albedo * (light_data[i].ambient.xyz + light_data[i].diffuse.xyz * diffuse * shadow);
	}
	return color;
}

// distance along the camera's forward axis of the opaque scene at this pixel
float scene_view_depth()
{
	float depth = texelFetch(scene_depth, ivec2(gl_FragCoord.xy), 0).r;
	vec4 v = inverse_projection * vec4(0.0, 0.0, depth * 2.0 - 1.0, 1.0);
	return -v.z / v.w;
}

void main()
{
	vec4 texel;
	if (has_texture != 0u)
	{
		texel = mix(texture(particle_texture, fs_in.frame_uv), texture(particle_texture, fs_in.next_frame_uv), fs_in.frame_blend);
	}
	else
	{
		// the built-in soft circle, white (premultiplied for the premultiplied mode)
		float a = clamp(1.0 - length(fs_in.uv - vec2(0.5)) * 2.0, 0.0, 1.0);
		a = a * a * (3.0 - 2.0 * a);
		texel = vec4(blend_mode == 1u ? vec3(a) : vec3(1.0), a);
	}

	// erosion: the texture's alpha is its density; the thinner parts go first, the rest keeps its full strength
	if (fs_in.erosion > 0.0)
	{
		float eroded = clamp((texel.a - fs_in.erosion) / max(1.0 - fs_in.erosion, 1e-3), 0.0, 1.0);
		// premultiplied colour scales with its alpha
		if (blend_mode == 1u && texel.a > 0.0)
		{
			texel.rgb *= eroded / texel.a;
		}
		texel.a = eroded;
	}

	vec4 color = fs_in.color;
	if (lit != 0u)
	{
		color.rgb = light_particle(color.rgb);
	}
	if (soft_distance > 0.0)
	{
		color.a *= clamp((scene_view_depth() - fs_in.view_depth) / soft_distance, 0.0, 1.0);
	}

	if (blend_mode == 1u)
	{
		out_fragment_color = vec4(texel.rgb * color.rgb * color.a, texel.a * color.a);
	}
	else if (blend_mode == 2u)
	{
		vec4 c = texel * color;
		out_fragment_color = vec4(c.rgb * c.a, 0.0);
	}
	else
	{
		out_fragment_color = texel * color;
	}
	if (out_fragment_color.a <= 0.0 && blend_mode != 2u)
	{
		discard;
	}
}
