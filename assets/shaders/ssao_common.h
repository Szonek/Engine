// Shared by ssao.fs and ssao_blur.fs. How it works: src/engine/impl/render/SHADOWS_AND_SSAO.md

#define SSAO_MAX_SAMPLES 32

// has to match SsaoPass::GpuData (ssao_pass.h)
layout (std140) uniform SsaoData
{
	mat4 projection;
	mat4 inverse_projection;
	// offsets inside of a unit hemisphere pointing along +Z
	vec4 kernel[SSAO_MAX_SAMPLES];
	float radius;
	float bias;
	float power;
	int sample_count;
};

// depth buffer of the scene
layout(binding = 0) uniform sampler2D texture_depth;

// Depth buffer stores only the depth, but together with the pixel coordinates that's a full position:
// it's turned to NDC (<-1; 1>) and the projection is undone. Result is in view space (camera at 0, looking along -Z).
vec3 view_position_at(ivec2 pixel)
{
	ivec2 size = textureSize(texture_depth, 0);
	pixel = clamp(pixel, ivec2(0), size - 1);
	float depth = texelFetch(texture_depth, pixel, 0).r;
	vec2 uv = (vec2(pixel) + 0.5f) / vec2(size);
	vec4 view_pos = inverse_projection * vec4(vec3(uv, depth) * 2.0f - 1.0f, 1.0f);
	return view_pos.xyz / view_pos.w;
}
