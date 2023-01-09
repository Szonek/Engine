#version 420 core
in vec3 fragment_color;
in vec2 uv;

out vec4 out_fragment_color;


layout(binding=0) uniform sampler2D texture_0;
layout(binding=1) uniform sampler2D texture_1;

void main()
{
	out_fragment_color = mix(texture(texture_0, uv), texture(texture_1, uv), 0.5f);
} 