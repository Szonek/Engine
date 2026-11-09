// Particles: colour times texture (or a soft circle), eroded, optionally lit and soft, written for the batch's blend
// mode (ParticleBlend): 0 alpha (src alpha, 1 - src alpha), 1 premultiplied (one, 1 - src alpha; the texture is
// premultiplied), 2 additive (one, one).
in vec2 particle_uv;
in vec2 particle_uv_next;
in float particle_frame_blend;
in vec4 particle_color;
in float particle_erosion;
in vec2 particle_corner;
in vec3 particle_world_position;

uniform mat4 view;
uniform mat4 projection;
uniform uint blend_mode;
uniform uint has_texture;
// 0 rgba: a colour and alpha; 1 packed smoke: R brightness, G erosion noise, A alpha
uniform uint packed_channels;
uniform float erosion_softness;
uniform uint lit;
uniform float light_wrap;
// 0: no soft edge (the scene's depth isn't read)
uniform float soft_distance;

layout(binding = ENGINE_BINDING_SLOT_TEXTURE_DIFFUSE) uniform sampler2D particle_texture;
// a copy of the scene's depth (Scene Depth Copy), for soft particles
layout(binding = ENGINE_BINDING_SLOT_TEXTURE_SCENE_DEPTH) uniform sampler2D texture_scene_depth;

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

// the distance in front of the camera of a depth buffer value (0..1), for perspective and orthographic projections
float view_distance(float depth)
{
	float z_ndc = depth * 2.0 - 1.0;
	float z_view = (projection[3][2] - z_ndc * projection[3][3]) / (z_ndc * projection[2][3] - projection[2][2]);
	return -z_view;
}

void main()
{
	vec4 texel;
	if (has_texture != 0u)
	{
		texel = mix(texture(particle_texture, particle_uv), texture(particle_texture, particle_uv_next), particle_frame_blend);
	}
	else
	{
		// the built-in soft circle, white
		float a = clamp(1.0 - length(particle_corner) * 2.0, 0.0, 1.0);
		texel = vec4(1.0, 1.0, 1.0, a * a * (3.0 - 2.0 * a));
	}
	// what the texture holds: a colour, or a smoke puff's brightness; and what erosion thresholds
	vec3 albedo = packed_channels != 0u ? vec3(texel.r) : texel.rgb;
	float alpha = texel.a;
	float erosion_noise = packed_channels != 0u ? texel.g : texel.a;
	if (particle_erosion > 0.0)
	{
		alpha *= smoothstep(particle_erosion, particle_erosion + erosion_softness, erosion_noise);
	}
	albedo *= particle_color.rgb;
	alpha *= particle_color.a;

	if (lit != 0u && direction_light_count > 0u)
	{
		// a sphere-shaped normal: the puff bulges toward the camera, so its side toward the light is bright
		vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
		vec3 up = vec3(view[0][1], view[1][1], view[2][1]);
		vec3 toward_camera = vec3(view[0][2], view[1][2], view[2][2]);
		vec2 p = particle_corner * 2.0;
		vec3 normal = normalize(right * p.x + up * p.y + toward_camera * sqrt(max(0.0, 1.0 - dot(p, p))));
		LightPacket light = light_data[0];
		vec3 to_light = normalize(light.direction.xyz);
		// wrap lighting: the light reaches a little around the puff, like through thin smoke
		float diffuse = max(0.0, (dot(normal, to_light) + light_wrap) / (1.0 + light_wrap));
		float view_depth = -(view * vec4(particle_world_position, 1.0)).z;
		float shadow = calc_directional_shadow(particle_world_position, normal, to_light, view_depth);
		albedo *= light.ambient.xyz + light.diffuse.xyz * diffuse * shadow;
	}

	if (soft_distance > 0.0)
	{
		// fades out where the geometry behind is close
		float scene_depth = texelFetch(texture_scene_depth, ivec2(gl_FragCoord.xy), 0).r;
		float gap = view_distance(scene_depth) - view_distance(gl_FragCoord.z);
		alpha *= clamp(gap / soft_distance, 0.0, 1.0);
	}

	if (alpha <= 0.0)
	{
		discard;
	}
	if (blend_mode == 1u)
	{
		// the texture is premultiplied: only the particle's own alpha is applied to its colour
		out_fragment_color = vec4(albedo * particle_color.a, alpha);
	}
	else if (blend_mode == 2u)
	{
		out_fragment_color = vec4(albedo * alpha, 0.0);
	}
	else
	{
		out_fragment_color = vec4(albedo, alpha);
	}
}
