#version 430 core

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

	gl_Position = projection * view * vec4(world_position, 1.0f);
	gl_Position /= gl_Position.w;
	gl_Position.xy += positions[gl_VertexID].xy * scale.xy;
	
    texture_uv = coords[gl_VertexID];
}  