#version 320 es

out mediump vec4 out_fragment_color;

uniform mediump vec4 color;

void main()
{
	out_fragment_color = color;
} 