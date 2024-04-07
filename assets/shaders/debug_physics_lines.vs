void main()
{
	vec3 world_position = in_vertex_position;
	gl_Position = projection * view * vec4(world_position, 1.0f);
}