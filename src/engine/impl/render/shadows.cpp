#include "shadows.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

engine::DirectionalShadow engine::compute_directional_shadow(const glm::mat4& camera_view, const glm::mat4& camera_projection, const glm::vec3& direction_to_light, const ShadowSettings& settings)
{
    // 1. Corners of the camera frustum in world space, far plane pulled in to the shadow distance.
    //    Unprojecting the NDC cube works for both perspective and orthographic cameras.
    const auto inverse_view_projection = glm::inverse(camera_projection * camera_view);
    const auto unproject = [&](float x, float y, float z)
    {
        const auto world = inverse_view_projection * glm::vec4(x, y, z, 1.0f);
        return glm::vec3(world) / world.w;
    };
    const auto view_depth = [&](const glm::vec3& world)
    {
        return -(camera_view * glm::vec4(world, 1.0f)).z;
    };

    std::array<glm::vec3, 8> corners{};
    std::size_t corners_count = 0;
    for (const float x : { -1.0f, 1.0f })
    {
        for (const float y : { -1.0f, 1.0f })
        {
            const auto near_corner = unproject(x, y, -1.0f);
            const auto far_corner = unproject(x, y, 1.0f);
            // view depth changes linearly along the edge between the near and the far corner
            const auto near_depth = view_depth(near_corner);
            const auto far_depth = view_depth(far_corner);
            const auto t = std::clamp((settings.distance - near_depth) / std::max(far_depth - near_depth, 1e-6f), 0.0f, 1.0f);
            corners[corners_count++] = near_corner;
            corners[corners_count++] = glm::mix(near_corner, far_corner, t);
        }
    }

    // 2. Bounding sphere of the corners. A sphere (unlike a tight box) has the same size no matter how the camera is rotated,
    //    so the shadow map always covers the same area and shadows don't change resolution when the camera turns.
    glm::vec3 center(0.0f);
    for (const auto& corner : corners)
    {
        center += corner;
    }
    center /= static_cast<float>(corners.size());
    float radius = 0.0f;
    for (const auto& corner : corners)
    {
        radius = std::max(radius, glm::length(corner - center));
    }
    // rounded up, so float noise doesn't change the size from frame to frame
    radius = std::ceil(radius * 16.0f) / 16.0f;

    // 3. Light space: rotation only for now, the light looks along its direction.
    auto to_light = direction_to_light;
    if (glm::dot(to_light, to_light) < 1e-8f)
    {
        to_light = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    const auto light_forward = -glm::normalize(to_light);
    const auto up = std::abs(light_forward.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    const auto light_rotation = glm::lookAt(glm::vec3(0.0f), light_forward, up);

    // 4. Move the sphere center only in steps of one texel. Without it the texel grid slides over the world when the camera moves
    //    and shadow edges shimmer.
    DirectionalShadow shadow;
    shadow.world_texel_size = (2.0f * radius) / static_cast<float>(std::max(settings.map_size, 1u));
    auto center_light_space = glm::vec3(light_rotation * glm::vec4(center, 1.0f));
    center_light_space.x = std::floor(center_light_space.x / shadow.world_texel_size) * shadow.world_texel_size;
    center_light_space.y = std::floor(center_light_space.y / shadow.world_texel_size) * shadow.world_texel_size;
    shadow.view = glm::translate(glm::mat4(1.0f), -center_light_space) * light_rotation;

    // 5. Orthographic box around the sphere. In light space the light is at +Z, so the near plane is moved towards it
    //    to capture casters which are outside of the camera frustum but still shadow it.
    shadow.projection = glm::ortho(-radius, radius, -radius, radius, -(radius + settings.caster_distance), radius);
    return shadow;
}
