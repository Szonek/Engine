#version 430 core
#define FIXED_SIZE 1   // with this sprite will try to maintain it's size, if this is 0 it will be scaled (i.e. camera zoom out so the sprite will be smaller)


layout (binding = 0, std140) uniform CameraData
{
	mat4 view;
    mat4 projection;
	vec4 view_pos;
};
uniform vec3 world_position;
uniform vec3 scale;

out vec2 texture_uv;

void main()
{
    const vec2 positions[6] = vec2[]
	(  
        vec2(-0.5f,  0.5f),  
        vec2(-0.5f, -0.5f),  
        vec2( 0.5f, -0.5f),  
        
        vec2(-0.5f,  0.5f),  
        vec2( 0.5f, -0.5f),  
        vec2( 0.5f,  0.5f) 
    );
	
	const vec2 coords[6] = vec2[]
	(
		vec2(0.0f, 1.0f),
		vec2(0.0f, 0.0f),
		vec2(1.0f, 0.0f),
		
		vec2(0.0f, 1.0f),
		vec2(1.0f, 0.0f),
		vec2(1.0f, 1.0f)	
	);
	
	vec3 CameraRight_worldspace = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 CameraUp_worldspace = vec3(view[0][1], view[1][1], view[2][1]);

#if FIXED_SIZE
	gl_Position = projection * view * vec4(world_position, 1.0f);
	gl_Position /= gl_Position.w;
	gl_Position.xy += positions[gl_VertexID].xy * scale.xy;
#else
    vec3 world_pos_adjusted_to_camera = world_position
		+ CameraRight_worldspace * positions[gl_VertexID].x * scale.x	
		+ CameraUp_worldspace * positions[gl_VertexID].y * scale.y;	
	
	gl_Position = projection * view * vec4(world_pos_adjusted_to_camera, 1.0f);
#endif
    texture_uv = coords[gl_VertexID];
}  