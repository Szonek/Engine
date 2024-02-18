#version 320 es

#define USE_SKINNING 1
#define MAX_BONES 128

layout (location = 0) in vec3 in_vertex_position;
layout (location = 1) in vec2 in_vertex_tex_coord;
layout (location = 2) in vec3 normals;
layout (location = 3) in uvec4 joints_0;
layout (location = 4) in vec4 weights_0;

out mediump vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform mat4 joints_mat[MAX_BONES];

void main()
{
#if USE_SKINNING
    mat4 skin_mat = 
        weights_0.x * joints_mat[int(joints_0.x)] +
        weights_0.y * joints_mat[int(joints_0.y)] +
        weights_0.z * joints_mat[int(joints_0.z)] +
        weights_0.w * joints_mat[int(joints_0.w)];
    vec4 world_position = skin_mat * vec4(in_vertex_position,1.0);
#else
    vec4 world_position = vec4(in_vertex_position, 1.0);
#endif
	gl_Position = projection * view * model * world_position;
	//gl_Position = projection * view * world_position;
	uv = in_vertex_tex_coord;
}