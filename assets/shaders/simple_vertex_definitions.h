#version 430

layout (location = 0) in mediump vec3 in_vertex_position;
layout (location = 1) in mediump vec2 in_vertex_tex_coord;
layout (location = 2) in mediump vec3 in_normals;
layout (location = 3) in mediump uvec4 in_bone_id_0;
layout (location = 4) in mediump vec4 in_weights_0;

out VS_OUT
{
    vec2 uv;
    vec3 normals;
	vec3 world_pos;
} vs_out;

layout (binding = 1, std140) uniform CameraData
{
	mat4 view;
    mat4 projection;
	vec4 view_pos;
};
uniform mat4 model;
