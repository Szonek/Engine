#version 320 es

in mediump vec2 out_uv;

out mediump vec4 out_fragment_color;

uniform mediump vec4 diffuse_color;
layout(binding=1) uniform sampler2D texture_diffuse;

void main()
{
	mediump vec4 diffuse_texture_color = texture(texture_diffuse, out_uv);
	out_fragment_color = diffuse_texture_color * diffuse_color;
} 