#version 330 core
layout (location = 0) in vec3 in_vertex_position;
layout (location = 1) in vec3 in_vertex_color;
layout (location = 2) in vec2 in_vertex_tex_coord;

out vec3 fragment_color;
out vec2 uv;

uniform mat4 model_matrix;

void main()
{
	gl_Position = model_matrix * vec4(in_vertex_position, 1.0);
	fragment_color = in_vertex_color;
	uv = in_vertex_tex_coord;
}