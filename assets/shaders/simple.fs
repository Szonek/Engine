#version 320 es

in mediump vec2 uv;

out mediump vec4 out_fragment_color;

uniform mediump vec4 diffuse_color;
//layout(binding=0) uniform sampler2D texture_diffuse;

void main()
{
	//out_fragment_color = texture(texture_diffuse, uv);
	out_fragment_color = diffuse_color;
} 