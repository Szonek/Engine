#version 430 core



layout (binding = 0, std140) uniform CameraData
{
	mat4 view;
    mat4 projection;
	vec4 view_pos;
};
uniform mat4 model;

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
	
    gl_Position = projection * view * model * vec4(positions[gl_VertexID], 0.0, 1.0); 
    texture_uv = coords[gl_VertexID];
}  