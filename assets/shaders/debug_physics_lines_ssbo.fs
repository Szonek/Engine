#version 430

in vec3 line_color;
layout (location = 0) out vec4 out_fragment_color;
void main()
{
	out_fragment_color = vec4(line_color, 1.0f);
} 