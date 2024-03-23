#version 320 es

in mediump vec2 uv;

out mediump vec4 out_fragment_color;

uniform mediump vec4 diffuse_color;
//layout(binding=1) uniform sampler2D texture_diffuse;

void main()
{
	//mediump vec4 diffuse_texture_color = texture(texture_diffuse, uv);
	mediump vec4 diffuse_texture_color = vec4(1.0, 1.0, 1.0, 1.0);
	out_fragment_color = diffuse_texture_color * diffuse_color;
} 