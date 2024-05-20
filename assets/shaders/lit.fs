#version 430

in VS_OUT
{
    vec2 uv;
    vec3 normals;
	vec3 world_pos;
} fs_in;

layout (binding = 1, std140) uniform CameraData
{
	mat4 view;
    mat4 projection;
	vec4 view_pos;
};

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
	// fragment specific
	vec3 normal = normalize(fs_in.normals);
	vec3 view_dir = normalize(view_pos.xyz - fs_in.world_pos);
	vec4 frag_color = texture(texture_diffuse, fs_in.uv) * diffuse_color;
		
	// light specific
	vec3 light_dir = normalize(light_data[0].position.xyz - fs_in.world_pos); 
	
	// ambient
	vec4 ambient = light_data[0].ambient;
	
	// diffuse

	float diffuse_factor = max(dot(normal, light_dir), 0.0);
	vec4 diffuse = light_data[0].diffuse * diffuse_factor;
	
	// specular
	float specular_strength = 0.5f;
	vec3 reflect_dir = reflect(-light_dir, normal);
	float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
	vec4 specular = specular_strength * spec * light_data[0].specular;
	
	// final result
	out_fragment_color = (ambient + diffuse + specular) * frag_color;
} 