#version 430

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

vec3 calc_dir_light(LightPacket light, vec3 normal, vec3 view_dir, vec3 frag_color, vec3 specular_color, float shininess)
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

vec3 calc_point_light(LightPacket light, vec3 normal, vec3 view_dir, vec3 frag_world_pos, vec3 frag_color, vec3 specular_color, float shininess)
{
	vec3 light_dir = normalize(light.position.xyz - frag_world_pos); 
	float distance = length(light.position.xyz - frag_world_pos);
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

vec3 calc_spot_light(LightPacket light, vec3 normal, vec3 view_dir, vec3 frag_world_pos, vec3 frag_color, vec3 specular_color, float shininess)
{
	vec3 light_dir = normalize(light.position.xyz - frag_world_pos); 
	float distance = length(light.position.xyz - frag_world_pos);
	float light_constant = light.attenuation.x;
	float light_linear = light.attenuation.y * distance;
	float light_quadratic = light.attenuation.z * (distance * distance);
	float attenuation = 1.0f / (light_constant + light_linear * light_quadratic);
	
	// ambient - calc first as it's not affected by cut off values
	vec3 ambient = light.ambient.xyz * frag_color * attenuation;
	
	float theta = dot(light_dir, normalize(-light.direction.xyz));
	float cut_off = light.position.w; // we packed cutoff into "W" component of position
	
	// calculate intensity to simulate soft, smooth edges
	float cut_off_outer = light.direction.w; // we packed outer cutoff into "W" component of direction
	float epsilon   = cut_off - cut_off_outer;
	float intensity = clamp((theta - cut_off_outer) / epsilon, 0.0, 1.0);
	
	// diffuse
	float diffuse_factor = max(dot(normal, light_dir), 0.0);
	vec3 diffuse = light.diffuse.xyz * diffuse_factor * frag_color * intensity * attenuation;
	
	// specular
	vec3 reflect_dir = reflect(-light_dir, normal);
	float specular_factor = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);
	vec3 specular = light.specular.xyz * specular_factor * specular_color.xyz * intensity * attenuation;
	
	return ambient + diffuse + specular;
}  
