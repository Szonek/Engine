
in VS_OUT
{
    vec2 uv;
    vec3 normals;
	vec3 world_pos;
} fs_in;


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


uniform mediump vec4 diffuse_color;
uniform mediump float shininess;
layout(binding=5) uniform sampler2D texture_diffuse;
layout(binding=6) uniform sampler2D texture_specular;

void main()
{
	// fragment specific
	vec3 normal = normalize(fs_in.normals);
	vec3 view_dir = normalize(view_pos.xyz - fs_in.world_pos);
	vec3 frag_color = texture(texture_diffuse, fs_in.uv).xyz * diffuse_color.xyz;
	vec3 specular_color = texture(texture_specular, fs_in.uv).xyz;
	
	vec3 out_color = vec3(0.0f);
	vec3 ambient = vec3(0.0f);
	vec3 diffuse = vec3(0.0f);
	vec3 specular = vec3(0.0f);
	
	// directional
	for(uint i = 0; i < direction_light_count; i++)
	{
		out_color += calc_dir_light(light_data[i], normal, view_dir, frag_color, specular_color, shininess);
	}
	
	// point
	for(uint i = direction_light_count; i < direction_light_count + point_light_count; i++)
	{
		out_color += calc_point_light(light_data[i], normal, view_dir, fs_in.world_pos, frag_color, specular_color, shininess);
	}
	
	// spot
	for(uint i = direction_light_count + point_light_count; i < direction_light_count + point_light_count + spot_light_count; i++)
	{
		out_color += calc_spot_light(light_data[i], normal, view_dir, fs_in.world_pos, frag_color, specular_color, shininess);
	}
	
	// final result
	out_fragment_color = vec4((out_color + ambient + diffuse + specular), 1.0f);
} 