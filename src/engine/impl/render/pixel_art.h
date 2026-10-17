#pragma once
#include <glm/glm.hpp>

#include <cstdint>

// Math of the low resolution ("pixel art") rendering. Free of any graphics API calls, so it can be unit tested.
// The idea is explained in PIXEL_ART.md.
namespace engine
{
// Extra texels rendered on each side of the scene targets: the camera snapping shifts the image by up to half a texel
// when it's scaled up, the margin is what becomes visible at the window edges.
inline constexpr std::uint32_t PIXEL_ART_MARGIN = 1;

// Window uv (0..1 over the window, 0,0 is the left bottom corner) -> uv in the scene targets: uv * scale + offset
struct PresentTransform
{
    glm::vec2 scale{ 1.0f };
    glm::vec2 offset{ 0.0f };
};

// Size of the scene targets. pixel_scale 1: the window size, otherwise 1/pixel_scale of it (rounded up) plus the margin.
glm::uvec2 compute_scene_target_size(glm::uvec2 window_size, std::uint32_t pixel_scale);

// Camera the scene targets are rendered with, and how the result is mapped to the window.
struct LowResCamera
{
    glm::mat4 view{ 1.0f };
    glm::mat4 projection{ 1.0f };
    glm::vec3 position{ 0.0f };  // world space, matches view
    PresentTransform present;
};

// view, projection: the camera as the game sees it (projection made for the window aspect).
// The projection is widened by the margin. If snap is set and the camera is orthographic, the camera is moved to the
// closest position where world positions fall on whole scene target texels (the texel grid stays fixed in the world,
// edges don't crawl while the camera moves) and the move is undone in present: the image on the screen is where the
// camera really is, only the texel grid lags behind by less than a texel.
// Window pixel -> scene target texel is the same with or without snapping, except for that sub-texel offset.
LowResCamera compute_low_res_camera(const glm::mat4& view, const glm::mat4& projection, glm::uvec2 window_size, std::uint32_t pixel_scale, bool snap);

}  // namespace engine
