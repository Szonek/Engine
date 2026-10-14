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

layout (binding = ENGINE_BINDING_SLOT_CAMERA_DATA, std140) uniform CameraData
{
	mat4 view;
    mat4 projection;
	vec4 view_pos;
};
uniform mat4 model;

// The same geometry is drawn by several passes (depth pre-pass, then the lit pass with depth test "less or equal").
// invariant guarantees that every shader using these definitions computes exactly the same depth for it.
invariant gl_Position;
