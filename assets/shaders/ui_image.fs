#version 320 es
in mediump vec2 uv;
out mediump vec4 out_fragment_color;

uniform mediump vec4 diffuse_color;

void main()
{	
    out_fragment_color = diffuse_color;
} 
