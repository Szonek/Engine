#version 430 core

out vec4 out_fragment_color;
  
in vec2 texture_uv;

void main()
{ 
    out_fragment_color = vec4(1.0f, 1.0f, 1.0f, 0.0f);
}