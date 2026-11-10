// A terrain is drawn chunk by chunk with one shared grid of vertices (in_grid_position: 0 .. chunk cells along x and z),
// displaced by the terrain's heights: a texture with one float per sample. The same shader computes the depth for
// every pass (see simple_vertex_definitions.h).
layout (location = 0) in mediump vec2 in_grid_position;

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
// the first sample of the chunk
uniform uvec2 chunk_origin;
// samples along x and z
uniform uvec2 sample_count;
// metres between samples
uniform float cell_size;
// metres of the local xz plane one repeat of the material's textures covers
uniform vec2 texture_tile_size;
layout(binding = ENGINE_BINDING_SLOT_TEXTURE_TERRAIN_HEIGHT) uniform sampler2D texture_heights;

invariant gl_Position;
