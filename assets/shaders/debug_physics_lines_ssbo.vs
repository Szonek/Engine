uniform mat4 view;
uniform mat4 projection;

struct DrawLinePacket
{
	vec4 world_positions[2];   //xyz from, pad, xyz to, pad
	vec3 color;
	uint owner;   // the game object picked by a click on the line, 0 for none
};

layout (binding = ENGINE_BINDING_SLOT_DEBUG_PHYSICS, std430) readonly buffer LinePacketSSBO
{
	DrawLinePacket data[];
};


out mediump vec3 line_color;
flat out uint line_owner;

void main()
{
	line_color = data[gl_InstanceID].color;
	line_owner = data[gl_InstanceID].owner;
	vec3 world_position = data[gl_InstanceID].world_positions[gl_VertexID].xyz;
	gl_Position = projection * view * vec4(world_position, 1.0f);
}