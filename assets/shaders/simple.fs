#version 320 es

in mediump vec2 uv;

out mediump vec4 out_fragment_color;

uniform mediump vec4 diffuse_color;
uniform mediump float border_width;
uniform mediump vec4  border_color;
layout(binding=1) uniform sampler2D texture_diffuse;

void main()
{
	mediump vec4 diffuse_texture_color = texture(texture_diffuse, uv);
	
	if((uv.x <= border_width || uv.x >= (1.0f - border_width)) ||
		(uv.y <= border_width || uv.y >= (1.0f - border_width)))
	{
		out_fragment_color = border_color;
	}
	else
	{
		out_fragment_color = diffuse_texture_color * diffuse_color;
	}

} 