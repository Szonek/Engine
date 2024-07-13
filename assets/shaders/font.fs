#version 430 core

in vec2 uv;
out vec4 out_fragment_color;

uniform sampler2D glyph_texture;
uniform vec4 glyph_color;

uniform vec2 atlas_size;
uniform vec2 glyph_size;
uniform vec2 glyph_normalized_start_offset_in_atlas;

vec2 get_atlas_uv_offset()
{
	vec2 atlas_uv = (uv * glyph_size) / atlas_size;
	atlas_uv += glyph_normalized_start_offset_in_atlas;
	return atlas_uv; 
}

void main()
{
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(glyph_texture, get_atlas_uv_offset()).r);
    out_fragment_color = glyph_color * sampled;
} 
