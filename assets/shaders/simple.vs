#version 320 es
layout (location = 0) in vec3 in_vertex_position;
layout (location = 1) in vec2 in_vertex_tex_coord;
layout (location = 2) in vec4 joints_0;
layout (location = 3) in vec4 weights_0;

out mediump vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(in_vertex_position, 1.0);
	uv = in_vertex_tex_coord;
}