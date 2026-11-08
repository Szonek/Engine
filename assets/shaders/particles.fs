// Unlit particles: colour times texture (or a soft circle), written for the batch's blend mode (ParticleBlend):
// 0 alpha (src alpha, 1 - src alpha), 1 premultiplied (one, 1 - src alpha; the texture is premultiplied),
// 2 additive (one, one).
in vec2 particle_uv;
in vec4 particle_color;

uniform uint blend_mode;
uniform uint has_texture;
uniform sampler2D particle_texture;

layout (location = 0) out vec4 out_fragment_color;

void main()
{
	vec4 texel;
	if (has_texture != 0u)
	{
		texel = texture(particle_texture, particle_uv);
	}
	else
	{
		// the built-in soft circle, white (premultiplied for the premultiplied mode)
		float a = clamp(1.0 - length(particle_uv - vec2(0.5)) * 2.0, 0.0, 1.0);
		a = a * a * (3.0 - 2.0 * a);
		texel = vec4(blend_mode == 1u ? vec3(a) : vec3(1.0), a);
	}
	if (blend_mode == 1u)
	{
		out_fragment_color = vec4(texel.rgb * particle_color.rgb * particle_color.a, texel.a * particle_color.a);
	}
	else if (blend_mode == 2u)
	{
		vec4 c = texel * particle_color;
		out_fragment_color = vec4(c.rgb * c.a, 0.0);
	}
	else
	{
		out_fragment_color = texel * particle_color;
	}
	if (out_fragment_color.a <= 0.0 && blend_mode != 2u)
	{
		discard;
	}
}
