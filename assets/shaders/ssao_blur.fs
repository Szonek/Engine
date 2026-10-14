// Removes the 4x4 noise pattern of ssao.fs by averaging 4x4 pixels.
// Pixels at a different depth are skipped, so occlusion doesn't leak over the edges of objects.

layout(binding = 1) uniform sampler2D texture_occlusion;

layout (location = 0) out float out_occlusion;

void main()
{
	ivec2 pixel = ivec2(gl_FragCoord.xy);
	ivec2 size = textureSize(texture_occlusion, 0);
	float center_z = view_position_at(pixel).z;

	float sum = 0.0f;
	float weight_sum = 0.0f;
	for (int y = -2; y < 2; y++)
	{
		for (int x = -2; x < 2; x++)
		{
			ivec2 tap = clamp(pixel + ivec2(x, y), ivec2(0), size - 1);
			float weight = abs(view_position_at(tap).z - center_z) < radius ? 1.0f : 0.0f;
			sum += texelFetch(texture_occlusion, tap, 0).r * weight;
			weight_sum += weight;
		}
	}
	// the center pixel is always included, so weight_sum is never 0
	out_occlusion = sum / weight_sum;
}
