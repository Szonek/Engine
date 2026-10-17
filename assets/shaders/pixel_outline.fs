// Outlines of the low resolution "pixel art" look (src/engine/impl/render/PIXEL_ART.md), one scene pixel wide:
//  - silhouette: a neighbouring pixel is clearly further away, so this pixel is the edge of the nearer object -> darker
//  - crease: normals of the neighbour differ and it's not in front of this pixel (convex edge between faces) -> brighter
out vec4 out_fragment_color;

layout(binding = 0) uniform sampler2D scene_color;
layout(binding = 1) uniform sampler2D scene_depth;
layout(binding = 2) uniform sampler2D scene_normals;  // view space normal * 0.5 + 0.5, alpha 0 where nothing was drawn
uniform mat4 inverse_projection;
uniform float outline_strength;
uniform float highlight_strength;
uniform float depth_threshold;    // view space units
uniform float normal_threshold;   // 1 - cos(angle)

// distance from the camera plane
float view_depth(ivec2 texel)
{
	float depth = texelFetch(scene_depth, texel, 0).r;
	vec4 view_pos = inverse_projection * vec4(0.0f, 0.0f, depth * 2.0f - 1.0f, 1.0f);
	return -view_pos.z / view_pos.w;
}

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	ivec2 size = textureSize(scene_color, 0);
	vec4 color = texelFetch(scene_color, texel, 0);
	vec4 normal_data = texelFetch(scene_normals, texel, 0);
	if (normal_data.a < 0.5f)
	{
		out_fragment_color = color;  // nothing was drawn here
		return;
	}
	vec3 normal = normal_data.xyz * 2.0f - 1.0f;
	float depth = view_depth(texel);

	float silhouette = 0.0f;
	float crease = 0.0f;
	const ivec2 neighbours[4] = ivec2[](ivec2(1, 0), ivec2(-1, 0), ivec2(0, 1), ivec2(0, -1));
	for (int i = 0; i < 4; i++)
	{
		ivec2 neighbour = clamp(texel + neighbours[i], ivec2(0), size - 1);
		vec4 neighbour_normal_data = texelFetch(scene_normals, neighbour, 0);
		// empty neighbour: background, infinitely far
		float depth_difference = neighbour_normal_data.a < 0.5f ? 1e9f : view_depth(neighbour) - depth;
		silhouette = max(silhouette, step(depth_threshold, depth_difference));

		vec3 neighbour_normal = neighbour_normal_data.xyz * 2.0f - 1.0f;
		float normal_difference = 1.0f - dot(normal, neighbour_normal);
		// only the pixel on the near side of the edge gets it, so the line is one pixel wide
		float on_near_side = step(0.0f, depth_difference) * (1.0f - step(depth_threshold, depth_difference));
		crease = max(crease, step(normal_threshold, normal_difference) * on_near_side);
	}

	vec3 result = silhouette > 0.5f
		? color.rgb * (1.0f - outline_strength)
		: color.rgb * (1.0f + highlight_strength * crease);
	out_fragment_color = vec4(result, color.a);
}
