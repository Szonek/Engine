// Particles (the Transparent pass): an instanced camera-facing quad per particle, read from the scene's particle SSBO
// (ParticleGpuData in render_scene_data.h), sorted back to front on the CPU.
uniform mat4 view;
uniform mat4 projection;
// the batch's first particle in the buffer
uniform uint first_particle;
// the flipbook's columns and rows (1, 1: a plain texture), and whether frames blend into the next
uniform uvec2 flipbook_size;
uniform uint flipbook_blend;

struct ParticlePacket
{
	vec4 position_size;  // xyz world position, w the quad's side
	vec4 color;
	vec4 rotation_erosion_frame;   // x radians around the view axis, y the erosion threshold, z the flipbook frame
};

layout (binding = ENGINE_BINDING_SLOT_PARTICLES, std430) readonly buffer ParticleSSBO
{
	ParticlePacket particles[];
};

const vec2 CORNERS[6] = vec2[6](vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));

out vec2 particle_uv;
// the next flipbook frame's uv and how much of it shows
out vec2 particle_uv_next;
out float particle_frame_blend;
out vec4 particle_color;
out float particle_erosion;
// where on the quad, rotated with it: -0.5..0.5 along the camera's right and up axes (the lit puff's normal)
out vec2 particle_corner;
out vec3 particle_world_position;

// the uv of `corner_uv` (0..1 over the quad, v up) in the flipbook frame `frame` (left to right, top to bottom; image
// row 0 is the top and v 0 in the texture)
vec2 frame_uv(vec2 corner_uv, uint frame)
{
	uvec2 cell = uvec2(frame % flipbook_size.x, frame / flipbook_size.x);
	return (vec2(cell) + vec2(corner_uv.x, 1.0 - corner_uv.y)) / vec2(flipbook_size);
}

void main()
{
	ParticlePacket p = particles[first_particle + uint(gl_InstanceID)];
	vec2 corner = CORNERS[gl_VertexID];
	float s = sin(p.rotation_erosion_frame.x);
	float c = cos(p.rotation_erosion_frame.x);
	vec2 rotated = vec2(corner.x * c - corner.y * s, corner.x * s + corner.y * c);
	vec2 offset = rotated * p.position_size.w;
	// the camera's right and up axes in world space (the rows of the view matrix)
	vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 up = vec3(view[0][1], view[1][1], view[2][1]);
	vec3 world_position = p.position_size.xyz + right * offset.x + up * offset.y;

	uint frames = flipbook_size.x * flipbook_size.y;
	float frame = p.rotation_erosion_frame.z;
	uint first = min(uint(floor(frame)), frames - 1u);
	vec2 corner_uv = corner + vec2(0.5);
	particle_uv = frame_uv(corner_uv, first);
	particle_uv_next = frame_uv(corner_uv, (first + 1u) % frames);
	particle_frame_blend = flipbook_blend != 0u ? fract(frame) : 0.0;
	particle_color = p.color;
	particle_erosion = p.rotation_erosion_frame.y;
	particle_corner = rotated;
	particle_world_position = world_position;
	gl_Position = projection * view * vec4(world_position, 1.0);
}
