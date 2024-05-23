#version 430

in VS_OUT
{
    vec2 uv;
    vec3 normals;
	vec3 world_pos;
} fs_in;

// This wastes memory and can be tighly packed for better performance
// ToDo: get rid of padding areas
struct LightPacket
{
	vec4 position;      // Point and Spot: XYZ: position;          Spot: W: cutoff
	vec4 direction;    // Directional and Spot: XYZ: direction;   Spot: W: outer_cutoff
	vec4 attenuation;  // Point and Spot: XYZ: constant, linear, quadratic
	vec4 ambient;  //xyz
	vec4 diffuse;  //xyz
	vec4 specular; //xyz
};

layout (binding = 0, std140) uniform CameraData
{
	mat4 view;
    mat4 projection;
	vec4 view_pos;
};
layout (binding = 1, std140) uniform SceneData
{
    uint direction_light_count;
    uint point_light_count;
    uint spot_light_count;
	float pad0_;
};
layout (binding = 2, std430) readonly buffer LightPacketSSBO
{
	LightPacket light_data[];
};

out mediump vec4 out_fragment_color;


uniform mediump vec3 diffuse_color;
uniform mediump float shininess;
layout(binding=5) uniform sampler2D texture_diffuse;
layout(binding=6) uniform sampler2D texture_specular;


vec3 calc_dir_light(LightPacket light, vec3 normal, vec3 view_dir, vec3 frag_color, vec3 specular_color)
{
	vec3 light_dir = normalize(light.direction.xyz); 
	
	// ambient
	vec3 ambient = light.ambient.xyz * frag_color;
	
	// diffuse
	float diffuse_factor = max(dot(normal, light_dir), 0.0);
	vec3 diffuse = light.diffuse.xyz * diffuse_factor * frag_color;
	
	// specular
	vec3 reflect_dir = reflect(-light_dir, normal);
	float specular_factor = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);
	vec3 specular = light.specular.xyz * specular_factor * specular_color;
	
	return ambient + diffuse + specular;
}  

vec3 calc_point_light(LightPacket light, vec3 normal, vec3 view_dir, vec3 frag_color, vec3 specular_color)
{
	// point light specfific 
	vec3 light_dir = normalize(light.position.xyz - fs_in.world_pos); 
	float distance = length(light.position.xyz - fs_in.world_pos);
	float light_constant = light.attenuation.x;
	float light_linear = light.attenuation.y * distance;
	float light_quadratic = light.attenuation.z * (distance * distance);
	float attenuation = 1.0f / (light_constant + light_linear * light_quadratic);
	
	// ambient
	vec3 ambient = light.ambient.xyz * frag_color * attenuation;
	
	// diffuse
	float diffuse_factor = max(dot(normal, light_dir), 0.0);
	vec3 diffuse = light.diffuse.xyz * diffuse_factor * frag_color * attenuation;
	
	// specular
	vec3 reflect_dir = reflect(-light_dir, normal);
	float specular_factor = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);
	vec3 specular = light.specular.xyz * specular_factor * specular_color.xyz * attenuation;
	
	return ambient + diffuse + specular;
}  


void main()
{
	// fragment specific
	vec3 normal = normalize(fs_in.normals);
	vec3 view_dir = normalize(view_pos.xyz - fs_in.world_pos);
	vec3 frag_color = texture(texture_diffuse, fs_in.uv).xyz * diffuse_color;
	vec3 specular_color = texture(texture_specular, fs_in.uv).xyz;
	
	vec3 out_color = vec3(0.0f);
	vec3 ambient = vec3(0.0f);
	vec3 diffuse = vec3(0.0f);
	vec3 specular = vec3(0.0f);
	
	// directional
	for(uint i = 0; i < direction_light_count; i++)
	{
		out_color += calc_dir_light(light_data[i], normal, view_dir, frag_color, specular_color);
	}
	
	// point
	for(uint i = direction_light_count; i < direction_light_count + point_light_count; i++)
	{
		out_color += calc_point_light(light_data[i], normal, view_dir, frag_color, specular_color);
	}
	
	// final result
	out_fragment_color = vec4((out_color + ambient + diffuse + specular), 1.0f);
} 