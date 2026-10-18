#include "pixel_art.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

std::uint32_t engine::compute_pixel_scale(std::uint32_t window_height, std::uint32_t target_height)
{
    if (target_height == 0)
    {
        return 1;
    }
    const auto scale = std::lround(static_cast<double>(window_height) / static_cast<double>(target_height));
    return static_cast<std::uint32_t>(std::clamp<long>(scale, 1, PIXEL_ART_MAX_SCALE));
}

glm::uvec2 engine::compute_scene_target_size(glm::uvec2 window_size, std::uint32_t pixel_scale)
{
    if (pixel_scale <= 1)
    {
        return window_size;
    }
    const auto low_res = (window_size + glm::uvec2(pixel_scale - 1)) / pixel_scale;
    return low_res + glm::uvec2(2 * PIXEL_ART_MARGIN);
}

engine::LowResCamera engine::compute_low_res_camera(const glm::mat4& view, const glm::mat4& projection, glm::uvec2 window_size, std::uint32_t pixel_scale, bool snap)
{
    LowResCamera ret;
    ret.view = view;
    ret.projection = projection;
    ret.position = -glm::transpose(glm::mat3(view)) * glm::vec3(view[3]);
    if (pixel_scale <= 1 || window_size.x == 0 || window_size.y == 0)
    {
        return ret;
    }

    // The window shows window_size / pixel_scale texels (not a whole number in general), the targets have more:
    // shrinking the NDC by their ratio keeps the window's view in the middle of the targets and adds the margin around it.
    const auto target_size = glm::vec2(compute_scene_target_size(window_size, pixel_scale));
    const auto visible_texels = glm::vec2(window_size) / static_cast<float>(pixel_scale);
    const auto ndc_scale = visible_texels / target_size;
    ret.projection = glm::scale(glm::mat4(1.0f), glm::vec3(ndc_scale, 1.0f)) * projection;

    // movement of the camera in view space units which is undone in present
    glm::vec2 view_offset(0.0f);
    const bool is_orthographic = std::abs(projection[3][3] - 1.0f) < 1e-4f;
    if (snap && is_orthographic)
    {
        // size of one texel in view space units: the window covers 2 / projection[i][i] units
        const auto texel_size = glm::vec2(2.0f / projection[0][0], 2.0f / projection[1][1]) / visible_texels;
        // the translation of the view matrix is the world origin in view space, moving it by whole texels keeps
        // every world position at the same place relative to the texel grid
        const auto translation = glm::vec2(view[3]);
        const auto snapped = glm::round(translation / texel_size) * texel_size;
        view_offset = translation - snapped;
        ret.view[3][0] = snapped.x;
        ret.view[3][1] = snapped.y;
        ret.position = -glm::transpose(glm::mat3(ret.view)) * glm::vec3(ret.view[3]);
        ret.texel_size = texel_size;
    }

    // Window uv u shows what the original camera sees at NDC 2u-1. With the snapped camera that is drawn at
    // NDC (2u-1 - projection * view_offset) * ndc_scale of the targets.
    const auto ndc_offset = glm::vec2(projection[0][0], projection[1][1]) * view_offset;
    ret.present.scale = ndc_scale;
    ret.present.offset = 0.5f - 0.5f * ndc_scale - 0.5f * ndc_scale * ndc_offset;
    return ret;
}

glm::vec3 engine::compute_object_snap_offset(const LowResCamera& camera, const glm::vec3& origin)
{
    if (camera.texel_size.x <= 0.0f || camera.texel_size.y <= 0.0f)
    {
        return glm::vec3(0.0f);
    }
    // in view space the texel grid of the snapped camera is fixed: this is how far the origin is from it
    const auto view_origin = glm::vec2(camera.view * glm::vec4(origin, 1.0f));
    const auto snapped = glm::round(view_origin / camera.texel_size) * camera.texel_size;
    const auto view_offset = snapped - view_origin;

    // The object is moved horizontally (world XZ), never up or down: with the camera looking down at an angle, moving
    // it along the camera's up axis would also change its height, so a walking object would bob by a fraction of a texel
    // and its shadow on the ground would jitter. Kept at the same height, its shadow on flat ground stays at a constant
    // offset from it, so the shadow lands on the texel grid too and moves with it.
    // Solve right . d = view_offset.x, up . d = view_offset.y for d = (x, 0, z):
    const auto right = glm::vec3(camera.view[0][0], camera.view[1][0], camera.view[2][0]);
    const auto up = glm::vec3(camera.view[0][1], camera.view[1][1], camera.view[2][1]);
    const float determinant = right.x * up.z - right.z * up.x;
    if (std::abs(determinant) < 1e-3f)
    {
        // camera looks horizontally: moving on the ground can't move the object up or down on the screen
        return right * view_offset.x + up * view_offset.y;
    }
    return glm::vec3(
        (view_offset.x * up.z - right.z * view_offset.y) / determinant,
        0.0f,
        (right.x * view_offset.y - up.x * view_offset.x) / determinant);
}
