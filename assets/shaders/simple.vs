void main()
{
	vec3 world_position = in_vertex_position;
	gl_Position = projection * view * model * vec4(world_position, 1.0f);
	out_uv = in_vertex_tex_coord;
}