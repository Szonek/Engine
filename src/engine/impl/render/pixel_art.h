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
inline constexpr std::uint32_t PIXEL_ART_MAX_SCALE = 8;

// Whole number scale which brings the window height closest to target_height scene pixels (1080 -> 3 and 1440 -> 4 for
// 360), so the image looks the same on every screen and all pixels are equally big. target_height 0: 1 (window resolution).
std::uint32_t compute_pixel_scale(std::uint32_t window_height, std::uint32_t target_height);

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
    // size of a scene target texel in view space units, 0: the camera isn't snapped (objects can't be snapped either)
    glm::vec2 texel_size{ 0.0f };
};

// view, projection: the camera as the game sees it (projection made for the window aspect).
// The projection is widened by the margin. If snap is set and the camera is orthographic, the camera is moved to the
// closest position where world positions fall on whole scene target texels (the texel grid stays fixed in the world,
// edges don't crawl while the camera moves) and the move is undone in present: the image on the screen is where the
// camera really is, only the texel grid lags behind by less than a texel.
// Window pixel -> scene target texel is the same with or without snapping, except for that sub-texel offset.
LowResCamera compute_low_res_camera(const glm::mat4& view, const glm::mat4& projection, glm::uvec2 window_size, std::uint32_t pixel_scale, bool snap);

// Moving objects need the same treatment as the camera: an object at a different fraction of a texel every frame is
// rasterized differently every frame, so its edges flicker while it moves. Returns the world space offset which moves
// origin (the object's snap origin) to the texel grid of the snapped camera; every vertex of the object is moved by it, so
// the object keeps its shape and moves in whole texels. The offset is horizontal (the object keeps its height, so its
// shadow on flat ground moves in whole texels with it), except for a camera looking horizontally. Zero if the camera
// isn't snapped.
glm::vec3 compute_object_snap_offset(const LowResCamera& camera, const glm::vec3& origin);

}  // namespace engine
