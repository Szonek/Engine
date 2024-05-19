#version 430

in VS_OUT
{
    vec2 uv;
    vec3 normals;
} fs_in;


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
	vec4 frag_color = texture(texture_diffuse, fs_in.uv) * diffuse_color;
	vec4 ambient = frag_color * light_data[0].ambient;
	out_fragment_color = ambient;
} 