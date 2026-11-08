// Particles (the Transparent pass): an instanced camera-facing quad per particle, read from the scene's particle SSBO
// (ParticleGpuData in render_scene_data.h), sorted back to front on the CPU.
uniform mat4 view;
uniform mat4 projection;
// the batch's first particle in the buffer
uniform uint first_particle;
// the texture's flipbook: columns and rows of frames, left to right, top to bottom (1 x 1: the whole texture)
uniform uvec2 flipbook_size;

struct ParticlePacket
{
	vec4 position_size;  // xyz world position, w the quad's side
	vec4 color;
	vec4 params;         // x radians around the view axis, y erosion, z frame + blend into the next, w next frame
};

layout (binding = ENGINE_BINDING_SLOT_PARTICLES, std430) readonly buffer ParticleSSBO
{
	ParticlePacket particles[];
};

const vec2 CORNERS[6] = vec2[6](vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));

out PARTICLE_OUT
{
	// in the quad, 0..1 with v down (as images are stored: their first row at v 0), for the built-in circle
	vec2 uv;
	// in the frame and the next one of the flipbook
	vec2 frame_uv;
	vec2 next_frame_uv;
	// the corner in the quad, rotated with it, -1..1 along the camera's right and up axes (the sphere normal)
	vec2 sphere;
	vec4 color;
	vec3 world_position;
	// distance along the camera's forward axis
	float view_depth;
	float erosion;
	float frame_blend;
} vs_out;

vec2 frame_uv_of(uint frame, vec2 uv)
{
	uvec2 cell = uvec2(frame % flipbook_size.x, frame / flipbook_size.x);
	return (vec2(cell) + uv) / vec2(flipbook_size);
}

void main()
{
	ParticlePacket p = particles[first_particle + uint(gl_InstanceID)];
	vec2 corner = CORNERS[gl_VertexID];
	float s = sin(p.params.x);
	float c = cos(p.params.x);
	vec2 rotated = vec2(corner.x * c - corner.y * s, corner.x * s + corner.y * c);
	vec2 offset = rotated * p.position_size.w;
	// the camera's right and up axes in world space (the rows of the view matrix)
	vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 up = vec3(view[0][1], view[1][1], view[2][1]);
	vec3 world_position = p.position_size.xyz + right * offset.x + up * offset.y;

	// the texture's top at the quad's top
	vec2 uv = vec2(corner.x + 0.5, 0.5 - corner.y);
	float frame = floor(p.params.z);
	vs_out.uv = uv;
	vs_out.frame_uv = frame_uv_of(uint(frame), uv);
	vs_out.next_frame_uv = frame_uv_of(uint(p.params.w), uv);
	vs_out.frame_blend = p.params.z - frame;
	vs_out.sphere = rotated * 2.0;
	vs_out.color = p.color;
	vs_out.world_position = world_position;
	vec4 view_position = view * vec4(world_position, 1.0);
	vs_out.view_depth = -view_position.z;
	vs_out.erosion = p.params.y;
	gl_Position = projection * view_position;
}
