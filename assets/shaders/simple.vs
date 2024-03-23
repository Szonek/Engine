#version 320 es

#define USE_SKINNING 1
#define MAX_BONES 128

layout (location = 0) in vec3 in_vertex_position;
layout (location = 1) in vec2 in_vertex_tex_coord;
layout (location = 2) in vec3 in_normals;
layout (location = 3) in uvec4 in_bone_id_0;
layout (location = 4) in vec4 in_weights_0;

out mediump vec2 out_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform mat4 global_bone_transform[MAX_BONES];

#if USE_SKINNING
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
#endif

void main()
{
#if USE_SKINNING
	// postions is a point: homegenous coordiate = 1.0f
	vec3 world_position = apply_bone_transform(vec4(in_vertex_position, 1.0));
	// normals is a vector: homegenous coordiate = 0.0f
	//vec3 normals = apply_bone_transform(vec4(in_normals, 0.0));
#else
    vec3 world_position = in_vertex_position;
#endif
	gl_Position = projection * view * model * vec4(world_position, 1.0f);
	out_uv = in_vertex_tex_coord;
}