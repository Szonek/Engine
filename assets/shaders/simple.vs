#version 330 core
layout (location = 0) in vec3 in_vertex_position;
layout (location = 1) in vec2 in_vertex_tex_coord;

out vec3 fragment_color;
out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	//gl_Position = projection * view * model * vec4(in_vertex_position, 1.0);
	gl_Position = projection * view * model * vec4(in_vertex_position, 1.0);
	fragment_color = vec3(0.4f, 0.3f, 0.2f);
	uv = in_vertex_tex_coord;
}