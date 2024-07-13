#version 430 core

out vec2 uv;

uniform mat4 model_matrix;
uniform mat4 projection;

void main()
{
    const vec2 positions[6] = vec2[]
	(  
        vec2(0.0f, 1.0f),  
        vec2(0.0f, 0.0f),  
        vec2(1.0f, 0.0f),  
        
        vec2(0.0f, 1.0f),  
        vec2(1.0f, 0.0f),  
        vec2(1.0f, 1.0f) 
    );
	
	const vec2 coords[6] = vec2[]
	(
		vec2(0.0f, 0.0f),
		vec2(0.0f, 1.0f),
		vec2(1.0f, 1.0f),
		
		vec2(0.0f, 0.0f),
		vec2(1.0f, 1.0f),
		vec2(1.0f, 0.0f)	
	);
	
	gl_Position = projection * model_matrix * vec4(positions[gl_VertexID], 0.0, 1.0);
	uv = coords[gl_VertexID];
}