#version 330 core
layout (location = 0) in vec2 in_vertex_position;
layout (location = 1) in vec2 in_vertex_tex_coord;

out vec2 texture_uv;

void main()
{
    gl_Position = vec4(in_vertex_position.x, in_vertex_position.y, 0.0, 1.0); 
    texture_uv = in_vertex_tex_coord;
}  