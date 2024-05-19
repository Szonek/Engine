#version 430

in mediump vec2 out_uv;
out mediump vec4 out_fragment_color;

uniform mediump vec4 diffuse_color;
layout(binding=2) uniform sampler2D texture_diffuse;

struct LightPacket
{
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
};

layout (binding = 3, std430) readonly buffer LightPacketSSBO
{
	LightPacket light_data[];
};



void main()
{
	mediump vec4 diffuse_texture_color = texture(texture_diffuse, out_uv);
	out_fragment_color = diffuse_texture_color * diffuse_color * light_data[0].diffuse;
} 