#version 320 es
in mediump vec2 uv;
out mediump vec4 out_fragment_color;

uniform sampler2D glyph_texture;
uniform mediump vec4 glyph_color;

uniform mediump vec2 atlas_size;
uniform mediump vec2 glyph_size;
uniform mediump vec2 glyph_normalized_start_offset_in_atlas;

mediump vec2 get_atlas_uv_offset()
{
	mediump vec2 atlas_uv = (uv * glyph_size) / atlas_size;
	atlas_uv += glyph_normalized_start_offset_in_atlas;
	return atlas_uv; 
}

void main()
{
    mediump vec4 sampled = vec4(1.0, 1.0, 1.0, texture(glyph_texture, get_atlas_uv_offset()).r);
    out_fragment_color = glyph_color * sampled;
} 
