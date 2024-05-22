#version 430

in VS_OUT
{
    vec2 uv;
    vec3 normals;
	vec3 world_pos;
} fs_in;

out mediump vec4 out_fragment_color;

uniform mediump vec3 diffuse_color;
layout(binding=1) uniform sampler2D texture_diffuse;

void main()
{
	mediump vec4 diffuse_texture_color = texture(texture_diffuse, fs_in.uv);
	out_fragment_color = diffuse_texture_color * vec4(diffuse_color, 1.0f);
} 