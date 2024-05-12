#version 430

uniform mat4 view;
uniform mat4 projection;

layout (std140) uniform LinePacketUBO
{
	vec4 world_positions[2];   //xyz from, pad, xyz to, pad
	vec3 color;
	float pad2_;
};
in mediump vec3 line_color;

void main()
{
	line_color = LinePacketUBO[gl_InstanceID].color;
	vec3 world_position = LinePacketUBO[gl_InstanceID].world_position[gl_VertexID].xyz;
	gl_Position = projection * view * vec4(world_position, 1.0f);
}