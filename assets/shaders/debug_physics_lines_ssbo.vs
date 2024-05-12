#version 430

uniform mat4 view;
uniform mat4 projection;

struct DrawLinePacket
{
	vec4 world_positions[2];   //xyz from, pad, xyz to, pad
	vec3 color;
	float pad2_;
};

layout (binding = 2, std430) readonly buffer LinePacketSSBO
{
	DrawLinePacket data[];
};
out mediump vec3 line_color;

void main()
{
	line_color = data[gl_InstanceID].color;
	vec3 world_position = data[gl_InstanceID].world_positions[gl_VertexID].xyz;
	gl_Position = projection * view * vec4(world_position, 1.0f);
}