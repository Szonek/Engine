// Screen space ambient occlusion. Output: 1 - nothing blocks ambient light, 0 - fully occluded.

// 4x4 random vectors, repeated over the screen
layout(binding = 1) uniform sampler2D texture_noise;

layout (location = 0) out float out_occlusion;

// There is no normal buffer, so the normal is computed from positions of the neighbouring pixels.
// From each pair (left/right, down/up) the neighbour closer in depth is used: it most likely lies on the same surface.
vec3 reconstruct_normal(ivec2 pixel, vec3 center)
{
	vec3 left = view_position_at(pixel + ivec2(-1, 0));
	vec3 right = view_position_at(pixel + ivec2(1, 0));
	vec3 down = view_position_at(pixel + ivec2(0, -1));
	vec3 up = view_position_at(pixel + ivec2(0, 1));

	vec3 dx = abs(left.z - center.z) < abs(right.z - center.z) ? center - left : right - center;
	vec3 dy = abs(down.z - center.z) < abs(up.z - center.z) ? center - down : up - center;
	return normalize(cross(dx, dy));
}

void main()
{
	ivec2 pixel = ivec2(gl_FragCoord.xy);
	ivec2 size = textureSize(texture_depth, 0);
	if (texelFetch(texture_depth, pixel, 0).r >= 1.0f)
	{
		out_occlusion = 1.0f; // background
		return;
	}

	vec3 position = view_position_at(pixel);
	vec3 normal = reconstruct_normal(pixel, position);

	// Basis which turns the kernel (hemisphere around +Z) to the hemisphere around the normal.
	// It's rotated around the normal by a random vector, different for each pixel of a 4x4 block:
	// every pixel uses different sample positions, which hides the low number of samples (blur removes the noise).
	vec3 random = vec3(texelFetch(texture_noise, pixel & 3, 0).xy * 2.0f - 1.0f, 0.0f);
	vec3 tangent = random - normal * dot(random, normal);
	if (dot(tangent, tangent) < 1e-4f)
	{
		tangent = cross(normal, vec3(0.0f, 0.0f, 1.0f));
	}
	tangent = normalize(tangent);
	mat3 tbn = mat3(tangent, cross(normal, tangent), normal);

	float occlusion = 0.0f;
	for (int i = 0; i < sample_count; i++)
	{
		vec3 sample_position = position + tbn * kernel[i].xyz * radius;

		// which pixel of the screen is the sample at?
		vec4 clip = projection * vec4(sample_position, 1.0f);
		if (clip.w <= 0.0f)
		{
			continue;
		}
		vec2 uv = clip.xy / clip.w * 0.5f + 0.5f;

		// what the camera really sees at that pixel. If it is closer than the sample, the sample is inside of some geometry.
		float scene_z = view_position_at(ivec2(uv * vec2(size))).z;
		float is_occluded = scene_z >= sample_position.z + bias ? 1.0f : 0.0f;
		// geometry far in front of the surface (i.e. a character in front of a distant wall) doesn't occlude it
		float range_check = smoothstep(0.0f, 1.0f, radius / max(abs(position.z - scene_z), 1e-5f));
		occlusion += is_occluded * range_check;
	}

	out_occlusion = pow(1.0f - occlusion / float(max(sample_count, 1)), power);
}
