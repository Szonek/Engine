void main()
{
	vec4 world_position = model * vec4(in_vertex_position, 1.0f);
	gl_Position = projection * view * world_position;
	vs_out.uv = in_vertex_tex_coord;
	vs_out.normals = in_normals;
	vs_out.world_pos = world_position.xyz;
}