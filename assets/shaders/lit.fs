
in VS_OUT
{
    vec2 uv;
    vec3 normals;
	vec3 world_pos;
} fs_in;


layout (binding = ENGINE_BINDING_SLOT_CAMERA_DATA, std140) uniform CameraData
{
	mat4 view;
    mat4 projection;
	vec4 view_pos;
};
layout (binding = ENGINE_BINDING_SLOT_SCENE_DATA, std140) uniform SceneData
{
    uint direction_light_count;
    uint point_light_count;
    uint spot_light_count;
	float pad0_;
};
layout (binding = ENGINE_BINDING_SLOT_LIGHTS_DATA, std430) readonly buffer LightPacketSSBO
{
	LightPacket light_data[];
};

layout (location = 0) out vec4 out_fragment_color; 
layout (location = 1) out uint out_entity_id; 

uniform vec4 diffuse_color;
uniform float shininess;
uniform uint entity_id;
layout(binding = ENGINE_BINDING_SLOT_TEXTURE_DIFFUSE) uniform sampler2D texture_diffuse;
layout(binding = ENGINE_BINDING_SLOT_TEXTURE_SPECULAR) uniform sampler2D texture_specular;

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
	
	// ambient occlusion always dims ambient light, direct light only as much as requested (see shadow_helpers.h)
	float ambient_occlusion = sample_ambient_occlusion();
	float direct_occlusion = mix(1.0f, ambient_occlusion, ssao_direct_light_occlusion);
	float shadow = 1.0f;

	// directional
	for(uint i = 0; i < direction_light_count; i++)
	{
		float direct_visibility = direct_occlusion;
		// only the first directional light casts shadows
		if (i == 0u)
		{
			shadow = calc_directional_shadow(fs_in.world_pos, normal, normalize(light_data[i].direction.xyz), distance(view_pos.xyz, fs_in.world_pos));
			direct_visibility *= shadow;
		}
		out_color += calc_dir_light(light_data[i], normal, view_dir, frag_color, specular_color, shininess, ambient_occlusion, direct_visibility);
	}
	
	// point
	for(uint i = direction_light_count; i < direction_light_count + point_light_count; i++)
	{
		out_color += calc_point_light(light_data[i], normal, view_dir, fs_in.world_pos, frag_color, specular_color, shininess, ambient_occlusion, direct_occlusion);
	}
	
	// spot
	for(uint i = direction_light_count + point_light_count; i < direction_light_count + point_light_count + spot_light_count; i++)
	{
		out_color += calc_spot_light(light_data[i], normal, view_dir, fs_in.world_pos, frag_color, specular_color, shininess, ambient_occlusion, direct_occlusion);
	}
	
	// debug views (editor: debug panel)
	if (debug_view == DEBUG_VIEW_AMBIENT_OCCLUSION)
	{
		out_color = vec3(ambient_occlusion);
	}
	else if (debug_view == DEBUG_VIEW_SHADOW)
	{
		out_color = vec3(shadow);
	}

	// final result
	out_fragment_color = vec4((out_color + ambient + diffuse + specular), 1.0f);
	out_entity_id = entity_id;
} 