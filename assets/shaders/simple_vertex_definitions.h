#version 320 es

layout (location = 0) in vec3 in_vertex_position;
layout (location = 1) in vec2 in_vertex_tex_coord;
layout (location = 2) in vec3 in_normals;
layout (location = 3) in uvec4 in_bone_id_0;
layout (location = 4) in vec4 in_weights_0;

out mediump vec2 out_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
