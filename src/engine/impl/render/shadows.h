#pragma once
#include "render_settings.h"

#include <glm/glm.hpp>

// Math of the directional light shadow map. Free of any graphics API calls, so it can be unit tested.
// The idea is explained in SHADOWS_AND_SSAO.md.
namespace engine
{
// "Camera" of the light: what the shadow map is rendered with.
struct DirectionalShadow
{
    glm::mat4 view{ 1.0f };        // world -> light space
    glm::mat4 projection{ 1.0f };  // orthographic
    // size of one shadow map texel in world units
    float world_texel_size = 0.0f;
};

// Fits the shadow map to the part of the camera frustum which is closer than ShadowSettings::distance.
// direction_to_light: from a surface towards the light (the convention of engine_directional_light_data_t::direction).
DirectionalShadow compute_directional_shadow(const glm::mat4& camera_view, const glm::mat4& camera_projection, const glm::vec3& direction_to_light, const ShadowSettings& settings);

}  // namespace engine
