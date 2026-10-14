// Screen-door transparency: discards part of the pixels in a fixed 4x4 (Bayer) pattern.
// Used for fading occluders (i.e. walls between the camera and the player) without blending, so it works with
// the depth pre-pass: both the pre-pass and the lit pass discard exactly the same pixels.
// 0 - opaque, 1 - invisible. Uniforms which are never set read as 0, so materials which don't use it stay opaque.
uniform float dither_fade;

void apply_dither_fade()
{
    if (dither_fade <= 0.0)
    {
        return;
    }
    const float bayer[16] = float[16](
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0);
    const ivec2 p = ivec2(gl_FragCoord.xy) % 4;
    const float threshold = (bayer[p.y * 4 + p.x] + 0.5) / 16.0;
    if (threshold < dither_fade)
    {
        discard;
    }
}
