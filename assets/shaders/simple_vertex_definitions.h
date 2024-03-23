#version 320 es

layout (location = 0) in mediump vec3 in_vertex_position;
layout (location = 1) in mediump vec2 in_vertex_tex_coord;
layout (location = 2) in mediump vec3 in_normals;
layout (location = 3) in mediump uvec4 in_bone_id_0;
layout (location = 4) in mediump vec4 in_weights_0;

out mediump vec2 out_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
