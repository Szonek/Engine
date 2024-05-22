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


uniform mediump vec3 diffuse_color;
uniform mediump float shininess;
layout(binding=5) uniform sampler2D texture_diffuse;
layout(binding=6) uniform sampler2D texture_specular;

struct LightPacket
{
	vec4 data; //direction or position based on type of light
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
	vec3 frag_color = texture(texture_diffuse, fs_in.uv).xyz;
	vec3 specular_color = texture(texture_specular, fs_in.uv).xyz;
		
	// light specific
	//vec3 light_dir = normalize(light_data[0].position.xyz - fs_in.world_pos); 
	vec3 light_dir = normalize(light_data[0].data.xyz); 
	
	// ambient
	vec3 ambient = light_data[0].ambient.xyz * diffuse_color;
	
	// diffuse

	float diffuse_factor = max(dot(normal, light_dir), 0.0);
	vec3 diffuse = light_data[0].diffuse.xyz * diffuse_factor * diffuse_color;
	
	// specular
	vec3 reflect_dir = reflect(-light_dir, normal);  // -light_dir?
	float specular_factor = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);
	vec3 specular = light_data[0].specular.xyz * specular_factor * specular_color.xyz;
	
	// final result
	out_fragment_color = vec4((ambient + diffuse + specular) * frag_color, 1.0f);
} 