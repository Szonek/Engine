// Copies the scene to the window. With low resolution rendering (src/engine/impl/render/PIXEL_ART.md) every scene texel
// covers a block of window pixels: the texel is picked without filtering, so the pixels stay hard.
out vec4 out_fragment_color;

in vec2 texture_uv;

uniform sampler2D screen_texture;
// window uv -> uv in screen_texture: texture_uv * uv_scale + uv_offset (PresentTransform in pixel_art.h)
uniform vec2 uv_scale;
uniform vec2 uv_offset;

void main()
{
	ivec2 size = textureSize(screen_texture, 0);
	vec2 uv = texture_uv * uv_scale + uv_offset;
	ivec2 texel = clamp(ivec2(floor(uv * vec2(size))), ivec2(0), size - 1);
	out_fragment_color = texelFetch(screen_texture, texel, 0);
}
