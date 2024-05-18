void main()
{
	vec4 world_position = model * vec4(in_vertex_position, 1.0f);
	gl_Position = projection * view * world_position;
	vs_out.uv = in_vertex_tex_coord;
	//vs_out.uv = vec2(in_vertex_tex_coord.x, 1.0 - in_vertex_tex_coord.y);
	vs_out.normals = mat3(transpose(inverse(model))) * in_normals; //ToDo: optimize by sending transpose(inverse) from CPU
	vs_out.world_pos = world_position.xyz;
}