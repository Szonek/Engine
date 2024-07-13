#version 430 core

layout (location = 0) in vec2 in_vertex_position;
layout (location = 1) in vec2 in_vertex_tex_coord;

out vec2 uv;

uniform mat4 model_matrix;
uniform mat4 projection;

void main()
{
	gl_Position = projection * model_matrix * vec4(in_vertex_position, 0.0, 1.0);
	uv = in_vertex_tex_coord;
}