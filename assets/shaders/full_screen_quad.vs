#version 330 core

out vec2 texture_uv;

void main()
{
#if 1
    const vec2 positions[6] = vec2[]
	(  
        vec2(-1.0f,  1.0f),  
        vec2(-1.0f, -1.0f),  
        vec2( 1.0f, -1.0f),  
        
        vec2(-1.0f,  1.0f),  
        vec2( 1.0f, -1.0f),  
        vec2( 1.0f,  1.0f) 
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
	
#else 
    const vec2 positions[4] = vec2[](
        vec2(-1, -1),
        vec2(+1, -1),
        vec2(-1, +1),
        vec2(+1, +1)
    );
    const vec2 coords[4] = vec2[](
        vec2(0, 0),
        vec2(1, 0),
        vec2(0, 1),
        vec2(1, 1)
    );
#endif
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0); 
    texture_uv = coords[gl_VertexID];
}  