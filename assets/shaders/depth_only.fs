// Used by passes which need only the depth of the geometry (shadow map, depth pre-pass).
// Depth is written by the rasterizer itself, only dithered fade has to drop the same pixels as the lit pass.
void main()
{
    apply_dither_fade();
}
