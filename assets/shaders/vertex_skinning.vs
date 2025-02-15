#define MAX_BONES 64
uniform mat4 global_bone_transform[MAX_BONES];


// tutorial: https://lisyarus.github.io/blog/graphics/2023/07/03/gltf-animation.html
// algorthim: linear blend skinning (or just "skinning)
vec3 apply_bone_transform(vec4 p) 
{
	vec3 result = vec3(0.0);
	for (int i = 0; i < 4; ++i)
	{
		mat4x3 bone_transform = mat4x3(global_bone_transform[in_bone_id_0[i]]);
		result += in_weights_0[i] * (bone_transform * p);
	}
	return result;
}

void main()
{
	// postions is a point: homegenous coordiate = 1.0f
	vec3 position = apply_bone_transform(vec4(in_vertex_position, 1.0));
	// normals is a vector: homegenous coordiate = 0.0f
	vec3 normals = apply_bone_transform(vec4(in_normals, 0.0));
	
	vec4 world_position = model * vec4(position, 1.0f);
	gl_Position = projection * view * world_position;
	vs_out.uv = in_vertex_tex_coord;
	vs_out.world_pos = world_position.xyz;
	vs_out.normals = mat3(transpose(inverse(model))) * normals;  //ToDo: optimize by sending transpose(inverse) from CPU
}