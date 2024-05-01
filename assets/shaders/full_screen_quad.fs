#version 330 core

out vec4 out_fragment_color;
  
in vec2 texture_uv;

uniform sampler2D screen_texture;

void main()
{ 
    out_fragment_color = texture(screen_texture, texture_uv);
}