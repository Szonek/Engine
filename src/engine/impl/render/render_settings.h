#pragma once
#include <glm/vec3.hpp>

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

// Stylized ("painted") look of the lit shaders. How it works: STYLIZED_LIGHTING.md (next to this file).
// Disabled: the lit shaders compute exactly the standard lighting. The defaults keep the overall brightness of the
// standard lighting with the light of the test level (ambient 0.45, diffuse 0.75).
struct StylizedLightingSettings
{
    static constexpr std::uint32_t MAX_BANDS = 4;

    bool enabled = false;
    // Hemisphere ambient: ambient light is multiplied by the sky color on surfaces facing up and by the ground color
    // on surfaces facing down (blended in between). White and white: standard ambient.
    glm::vec3 sky_ambient{ 0.8f, 0.85f, 1.0f };
    glm::vec3 ground_ambient{ 0.5f, 0.45f, 0.4f };
    // Light ramp: diffuse light falls into this many steps of equal height, 1: lit or unlit (two tones),
    // 0: no steps, the standard smooth falloff.
    std::uint32_t bands = 3;
    // width of the step edges, 0: hard edges (cel shading), 1: the steps blend into each other
    float band_softness = 0.3f;
    // Tinted shadows: part of the directional light that surfaces it doesn't reach (facing away or in shadow) still get,
    // so shadows are colored instead of only darker. Black: standard shadows.
    glm::vec3 shadow_color{ 0.05f, 0.07f, 0.16f };
    // Rim light: bright edge where the surface turns away from the camera, separates characters from the ground.
    glm::vec3 rim_color{ 1.0f, 0.95f, 0.85f };
    // 0: no rim light
    float rim_strength = 0.2f;
    // higher: thinner rim
    float rim_power = 4.0f;
    // false: rim only on skinned meshes (characters). With a top-down camera every wall is seen at a grazing angle,
    // so a rim on static meshes lights up all vertical surfaces.
    bool rim_on_static_meshes = false;
};

// Low resolution rendering with hard pixels and outlines (t3ssel8r style). How it works: PIXEL_ART.md (next to this file).
struct PixelArtSettings
{
    // Height of the scene in pixels, 0: the scene is rendered at the window resolution. Otherwise it's rendered at 1/N of
    // the window, scaled up with hard (nearest) pixels: every scene pixel is an NxN block on the screen. N is the whole
    // number closest to window height / target_height (1080p -> 3, 1440p -> 4, 2160p -> 6 for 360), so the look is the
    // same on every screen. UI is not affected.
    std::uint32_t target_height = 0;
    // Orthographic cameras are moved by whole pixels of the scene, the rest of the movement is applied when the image is
    // scaled up. Without it edges crawl and flicker whenever the camera moves.
    bool snap_camera = true;

    // Outlines, computed from depth and normals of the scene:
    bool outline = false;
    // silhouettes: the pixel of the nearer object is darkened by this much (0..1)
    float outline_strength = 0.5f;
    // creases (convex edges between faces): the pixel is brightened by this much
    float highlight_strength = 0.3f;
    // how much further away a neighbouring pixel has to be to count as a silhouette (world units)
    float outline_depth_threshold = 0.3f;
    // how different the normals of neighbouring pixels have to be to count as a crease: 1 - cos(angle between them)
    float highlight_normal_threshold = 0.25f;
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
    StylizedLightingSettings stylized;
    PixelArtSettings pixel_art;
    RenderDebugView debug_view = RenderDebugView::eNone;
};

// Everything for the low resolution "pixel art" look: 360 scene lines (1/3 of 1080p), snapped camera, outlines, hard two step light with
// colored shadows, hard shadow edges. Other settings are kept.
inline void apply_pixel_art_preset(RenderSettings& settings)
{
    settings.pixel_art = PixelArtSettings{ .target_height = 360, .snap_camera = true, .outline = true };
    settings.shadows.pcf_radius = 0;
    settings.stylized = StylizedLightingSettings{
        .enabled = true,
        .bands = 2,
        .band_softness = 0.05f,
        .shadow_color = glm::vec3(0.07f, 0.08f, 0.2f),
        .rim_strength = 0.0f };
}

}  // namespace engine
