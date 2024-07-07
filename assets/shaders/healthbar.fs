#version 430 core

out vec4 out_fragment_color;
  
in vec2 texture_uv;

layout (binding = 1, std140) uniform UserData
{
	float fill_ratio;
	float pad0_;
	float pad1_;
	float pad2_;
};

void main()
{ 
	vec4 color = vec4(1.0f, 1.0f, 1.0f, 0.0f);
	if(texture_uv.x <= fill_ratio)
	{
		color.r = 0.0f;
		color.g = 1.0f;
		color.b = 0.0f;
	}
    out_fragment_color = color;
}