#version 430 core

out vec4 out_fragment_color;
  
in vec2 texture_uv;
uniform vec4 color;

void main()
{ 
    out_fragment_color = color;
}