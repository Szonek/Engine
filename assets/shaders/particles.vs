// Particles (the Transparent pass): an instanced camera-facing quad per particle, read from the scene's particle SSBO
// (ParticleGpuData in render_scene_data.h), sorted back to front on the CPU.
uniform mat4 view;
uniform mat4 projection;
// the batch's first particle in the buffer
uniform uint first_particle;

struct ParticlePacket
{
	vec4 position_size;  // xyz world position, w the quad's side
	vec4 color;
	vec4 rotation_pad;   // x radians around the view axis
};

layout (binding = ENGINE_BINDING_SLOT_PARTICLES, std430) readonly buffer ParticleSSBO
{
	ParticlePacket particles[];
};

const vec2 CORNERS[6] = vec2[6](vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));

out vec2 particle_uv;
out vec4 particle_color;

void main()
{
	ParticlePacket p = particles[first_particle + uint(gl_InstanceID)];
	vec2 corner = CORNERS[gl_VertexID];
	float s = sin(p.rotation_pad.x);
	float c = cos(p.rotation_pad.x);
	vec2 offset = vec2(corner.x * c - corner.y * s, corner.x * s + corner.y * c) * p.position_size.w;
	// the camera's right and up axes in world space (the rows of the view matrix)
	vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 up = vec3(view[0][1], view[1][1], view[2][1]);
	vec3 world_position = p.position_size.xyz + right * offset.x + up * offset.y;
	particle_uv = corner + vec2(0.5);
	particle_color = p.color;
	gl_Position = projection * view * vec4(world_position, 1.0);
}
