#pragma once
#include <cstdint>

namespace engine
{
// How shadows and SSAO work is described in SHADOWS_AND_SSAO.md (next to this file).

// Shadow of the first directional light of a scene.
struct ShadowSettings
{
    bool enabled = true;
    // width and height of the shadow map in texels
    std::uint32_t map_size = 2048;
    // shadows are rendered up to this distance from the camera (world units). Smaller distance = sharper shadows.
    float distance = 40.0f;
    // how far behind the visible area (towards the light) objects are still able to cast shadows into it
    float caster_distance = 50.0f;
    // constant offset in shadow map depth, removes remaining shadow acne
    float depth_bias = 0.0005f;
    // receiver is moved along its normal by this many shadow map texels before the lookup, removes acne on slopes
    float normal_offset = 1.5f;
    // 0: hard edge (one sample), N: (2N+1)x(2N+1) samples are averaged (PCF)
    std::uint32_t pcf_radius = 1;
    // 0: shadow is invisible, 1: shadowed surface gets no light from the directional light
    float strength = 1.0f;
};

struct SsaoSettings
{
    static constexpr std::uint32_t MAX_SAMPLES = 32;

    bool enabled = true;
    // radius of the sampled hemisphere (world units). Geometry further away doesn't occlude.
    float radius = 0.5f;
    // sample has to be at least this much (world units) in front of the surface to count, removes self occlusion
    float bias = 0.03f;
    // contrast of the result: ao = pow(ao, power)
    float power = 2.0f;
    std::uint32_t sample_count = 16;
    bool blur = true;
    // Ambient light is always occluded. This is how much of the occlusion is applied to direct light too:
    // 0 is the correct value, higher values make the effect more visible (stylized look).
    float direct_light_occlusion = 0.0f;
};

enum class RenderDebugView : std::uint32_t
{
    eNone = 0,
    eAmbientOcclusion = 1,  // lit geometry shows only the ambient occlusion term
    eShadow = 2             // lit geometry shows only the shadow term
};

struct RenderSettings
{
    ShadowSettings shadows;
    SsaoSettings ssao;
    RenderDebugView debug_view = RenderDebugView::eNone;
};

}  // namespace engine
