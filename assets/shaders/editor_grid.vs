// Full screen triangle. Each vertex carries the world space ends of the camera ray going through it (on the near and the far plane),
// the fragment shader intersects the interpolated ray with the ground plane. Works for perspective and orthographic cameras.

uniform mat4 inverse_view_projection;

out vec3 ray_near;
out vec3 ray_far;

vec3 unproject(vec3 ndc)
{
	vec4 world = inverse_view_projection * vec4(ndc, 1.0f);
	return world.xyz / world.w;
}

void main()
{
	const vec2 positions[3] = vec2[]
	(
		vec2(-1.0f, -1.0f),
		vec2( 3.0f, -1.0f),
		vec2(-1.0f,  3.0f)
	);

	vec2 position = positions[gl_VertexID];
	ray_near = unproject(vec3(position, -1.0f));
	ray_far = unproject(vec3(position, 1.0f));
	gl_Position = vec4(position, 0.0f, 1.0f);
}
