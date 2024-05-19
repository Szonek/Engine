#version 430

in VS_OUT
{
    vec2 uv;
    vec3 normals;
	vec3 world_pos;
} fs_in;


out mediump vec4 out_fragment_color;

uniform mediump vec4 diffuse_color;
layout(binding=2) uniform sampler2D texture_diffuse;

struct LightPacket
{
	vec4 position; //xyz
	vec4 ambient;  //xyz
	vec4 diffuse;  //xyz
	vec4 specular; //xyz
};

layout (binding = 3, std430) readonly buffer LightPacketSSBO
{
	LightPacket light_data[];
};



void main()
{
	vec3 normal = normalize(fs_in.normals);
	
	// ambient
	vec4 ambient = light_data[0].ambient;
	
	// diffuse
	vec3 light_dir = normalize(light_data[0].position.xyz - fs_in.world_pos); 
	float diffuse_factor = max(dot(normal, light_dir), 0.0);
	vec4 diffuse = light_data[0].diffuse * diffuse_factor;
	
	vec4 frag_color = texture(texture_diffuse, fs_in.uv) * diffuse_color;
	out_fragment_color = (ambient + diffuse) * frag_color;
} 