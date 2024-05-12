#version 320
#version 430

in mediump vec3 line_color;
out mediump vec4 out_fragment_color;

void main()
{
	out_fragment_color = vec4(line_color, 1.0f);
} 