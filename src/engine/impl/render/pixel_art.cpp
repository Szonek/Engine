#include "pixel_art.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

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
    }

    // Window uv u shows what the original camera sees at NDC 2u-1. With the snapped camera that is drawn at
    // NDC (2u-1 - projection * view_offset) * ndc_scale of the targets.
    const auto ndc_offset = glm::vec2(projection[0][0], projection[1][1]) * view_offset;
    ret.present.scale = ndc_scale;
    ret.present.offset = 0.5f - 0.5f * ndc_scale - 0.5f * ndc_scale * ndc_offset;
    return ret;
}
