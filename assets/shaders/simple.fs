#version 420 core
in vec3 fragment_color;
in vec2 uv;

out vec4 out_fragment_color;


layout(binding=0) uniform sampler2D texture_diffuse;

void main()
{
	out_fragment_color = texture(texture_diffuse, uv);
	//out_fragment_color = vec4(0.5f, 0.5f, 0.5f, 1.0f);
} 