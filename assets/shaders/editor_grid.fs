// Infinite grid on the XZ plane (y = 0), drawn by the editor (src/engine/impl/render/grid_feature.h).
//
// Lines are not geometry: for every pixel the camera ray is intersected with the plane and the distance from the hit point
// to the nearest grid line is compared with the size of the pixel in world units (screen space derivatives). That gives lines of
// constant width in pixels with smooth edges at any distance and angle.
// Cell size follows the zoom: there are three levels (x1, x10, x100), the smallest fades out before its lines get too dense.

in vec3 ray_near;
in vec3 ray_far;

uniform mat4 view_projection;

layout (location = 0) out vec4 out_fragment_color;

const float CELL_SIZE = 1.0f;               // world units, the smallest cell
const float MIN_PIXELS_BETWEEN_LINES = 8.0f;
const float LINE_WIDTH = 1.0f;              // pixels
const float AXIS_LINE_WIDTH = 2.0f;         // pixels
const vec4 COLOR_THIN = vec4(0.55f, 0.55f, 0.55f, 0.30f);
const vec4 COLOR_THICK = vec4(0.70f, 0.70f, 0.70f, 0.55f);
const vec3 COLOR_AXIS_X = vec3(0.90f, 0.25f, 0.30f);
const vec3 COLOR_AXIS_Z = vec3(0.25f, 0.45f, 0.95f);

// Coverage <0; 1> of a line of given width (pixels) by the pixel.
// distance_to_line and pixel_size are in the same units, per axis.
vec2 line_coverage(vec2 distance_to_line, vec2 pixel_size, float width)
{
	vec2 half_width = 0.5f * width * pixel_size;
	vec2 aa = 0.75f * pixel_size;
	return 1.0f - smoothstep(half_width - aa, half_width + aa, distance_to_line);
}

float grid_coverage(vec2 position, vec2 pixel_size, float cell_size)
{
	vec2 distance_to_line = abs(fract(position / cell_size - 0.5f) - 0.5f) * cell_size;
	vec2 coverage = line_coverage(distance_to_line, pixel_size, LINE_WIDTH);
	// lines which are denser than pixels would turn into noise, fade them to nothing instead
	coverage *= clamp(cell_size / (2.0f * pixel_size) - 1.0f, 0.0f, 1.0f);
	return max(coverage.x, coverage.y);
}

void main()
{
	vec3 ray = ray_far - ray_near;
	float t = -ray_near.y / ray.y;
	vec3 world_position = ray_near + t * ray;
	vec2 position = world_position.xz;

	// size of the pixel in world units, separately along X and Z
	vec2 ddx = dFdx(position);
	vec2 ddy = dFdy(position);
	vec2 pixel_size = vec2(length(vec2(ddx.x, ddy.x)), length(vec2(ddx.y, ddy.y)));

	// plane is behind the camera, beyond the far plane or parallel to the ray (NaN fails the comparison too)
	if (!(t > 0.0f && t < 1.0f))
	{
		discard;
	}

	// level of detail: 0 while the smallest cells are big enough on the screen, +1 for every 10x zoom out
	float max_pixel_size = max(pixel_size.x, pixel_size.y);
	float lod = max(0.0f, log(max_pixel_size * MIN_PIXELS_BETWEEN_LINES / CELL_SIZE) / log(10.0f) + 1.0f);
	float lod_fade = fract(lod);
	float cell_size_0 = CELL_SIZE * pow(10.0f, floor(lod));
	float cell_size_1 = cell_size_0 * 10.0f;
	float cell_size_2 = cell_size_1 * 10.0f;

	float coverage_0 = grid_coverage(position, pixel_size, cell_size_0);
	float coverage_1 = grid_coverage(position, pixel_size, cell_size_1);
	float coverage_2 = grid_coverage(position, pixel_size, cell_size_2);

	// the biggest visible level wins; the middle one turns from thin to thick while the smallest one disappears
	vec4 color = vec4(0.0f);
	if (coverage_2 > 0.0f)
	{
		color = vec4(COLOR_THICK.rgb, COLOR_THICK.a * coverage_2);
	}
	vec4 color_1 = mix(COLOR_THIN, COLOR_THICK, lod_fade);
	color_1.a *= coverage_1;
	vec4 color_0 = vec4(COLOR_THIN.rgb, COLOR_THIN.a * coverage_0 * (1.0f - lod_fade));
	if (color_1.a > color.a)
	{
		color = color_1;
	}
	if (color_0.a > color.a)
	{
		color = color_0;
	}

	// world axes: X is the line z = 0, Z is the line x = 0
	vec2 axis_coverage = line_coverage(abs(position), pixel_size, AXIS_LINE_WIDTH);
	color = mix(color, vec4(COLOR_AXIS_Z, 1.0f), axis_coverage.x);
	color = mix(color, vec4(COLOR_AXIS_X, 1.0f), axis_coverage.y);

	// fade out towards the horizon, where the plane is seen at a grazing angle
	float grazing = abs(normalize(ray).y);
	color.a *= smoothstep(0.0f, 0.15f, grazing);

	if (color.a <= 0.0f)
	{
		discard;
	}

	// Depth of the plane, so the grid is hidden by the geometry in front of it.
	// The point is pulled slightly towards the camera: the grid wins against a floor lying exactly at y = 0 instead of flickering.
	vec4 clip_position = view_projection * vec4(mix(world_position, ray_near, 0.002f), 1.0f);
	gl_FragDepth = clamp(clip_position.z / clip_position.w * 0.5f + 0.5f, 0.0f, 1.0f);

	out_fragment_color = color;
}
