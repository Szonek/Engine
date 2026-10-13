#include "render/shadows.h"

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <initializer_list>

namespace
{
using namespace engine;

glm::vec3 to_shadow_ndc(const DirectionalShadow& shadow, const glm::vec3& world)
{
    const auto clip = shadow.projection * shadow.view * glm::vec4(world, 1.0f);
    return glm::vec3(clip) / clip.w;
}

void expect_inside_shadow_map(const DirectionalShadow& shadow, const glm::vec3& world)
{
    const auto ndc = to_shadow_ndc(shadow, world);
    constexpr float epsilon = 1e-3f;
    EXPECT_LE(std::abs(ndc.x), 1.0f + epsilon) << "world: " << world.x << " " << world.y << " " << world.z;
    EXPECT_LE(std::abs(ndc.y), 1.0f + epsilon) << "world: " << world.x << " " << world.y << " " << world.z;
    EXPECT_LE(std::abs(ndc.z), 1.0f + epsilon) << "world: " << world.x << " " << world.y << " " << world.z;
}

// point which the camera sees at given NDC xy, at given distance in front of it
glm::vec3 visible_point(const glm::mat4& view, const glm::mat4& projection, float ndc_x, float ndc_y, float view_depth)
{
    const auto inverse_view_projection = glm::inverse(projection * view);
    const auto unproject = [&](float z)
    {
        const auto world = inverse_view_projection * glm::vec4(ndc_x, ndc_y, z, 1.0f);
        return glm::vec3(world) / world.w;
    };
    const auto near_point = unproject(-1.0f);
    const auto far_point = unproject(1.0f);
    const auto near_depth = -(view * glm::vec4(near_point, 1.0f)).z;
    const auto far_depth = -(view * glm::vec4(far_point, 1.0f)).z;
    return glm::mix(near_point, far_point, (view_depth - near_depth) / (far_depth - near_depth));
}

const glm::mat4 PERSPECTIVE = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
const glm::mat4 ORTHOGRAPHIC = glm::ortho(-16.0f, 16.0f, -9.0f, 9.0f, 0.1f, 1000.0f);
// camera of the game: above and behind the player
const glm::mat4 GAME_VIEW = glm::lookAt(glm::vec3(3.0f, 6.5f, 4.5f), glm::vec3(3.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}  // namespace

TEST(Shadows, VisibleAreaUpToShadowDistanceIsInsideOfTheShadowMap)
{
    ShadowSettings settings;
    settings.distance = 40.0f;
    for (const auto& projection : { PERSPECTIVE, ORTHOGRAPHIC })
    {
        for (const auto& to_light : { glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.4f, 1.0f, 0.3f), glm::vec3(-1.0f, 0.2f, 0.0f) })
        {
            const auto shadow = compute_directional_shadow(GAME_VIEW, projection, to_light, settings);
            for (const float x : { -1.0f, 0.0f, 1.0f })
            {
                for (const float y : { -1.0f, 0.0f, 1.0f })
                {
                    for (const float depth : { 0.1f, 10.0f, 40.0f })
                    {
                        expect_inside_shadow_map(shadow, visible_point(GAME_VIEW, projection, x, y, depth));
                    }
                }
            }
        }
    }
}

TEST(Shadows, CasterBetweenTheLightAndTheVisibleAreaIsInsideOfTheShadowMap)
{
    ShadowSettings settings;
    settings.distance = 40.0f;
    settings.caster_distance = 50.0f;
    const auto to_light = glm::normalize(glm::vec3(0.4f, 1.0f, 0.3f));
    const auto shadow = compute_directional_shadow(GAME_VIEW, PERSPECTIVE, to_light, settings);

    const auto receiver = visible_point(GAME_VIEW, PERSPECTIVE, 0.0f, 0.0f, 8.0f);
    expect_inside_shadow_map(shadow, receiver + to_light * 45.0f);
}

TEST(Shadows, CloserToTheLightMeansSmallerDepth)
{
    const auto to_light = glm::normalize(glm::vec3(0.4f, 1.0f, 0.3f));
    const auto shadow = compute_directional_shadow(GAME_VIEW, PERSPECTIVE, to_light, ShadowSettings{});

    const auto receiver = visible_point(GAME_VIEW, PERSPECTIVE, 0.0f, 0.0f, 8.0f);
    const auto caster = receiver + to_light * 2.0f;
    const auto receiver_ndc = to_shadow_ndc(shadow, receiver);
    const auto caster_ndc = to_shadow_ndc(shadow, caster);
    // both are on the same light ray: the same texel, caster in front
    EXPECT_NEAR(receiver_ndc.x, caster_ndc.x, 1e-4f);
    EXPECT_NEAR(receiver_ndc.y, caster_ndc.y, 1e-4f);
    EXPECT_LT(caster_ndc.z, receiver_ndc.z);
}

TEST(Shadows, CoveredAreaDoesNotDependOnCameraRotation)
{
    const ShadowSettings settings;
    const auto to_light = glm::vec3(0.4f, 1.0f, 0.3f);
    const auto eye = glm::vec3(3.0f, 6.5f, 4.5f);
    const auto reference = compute_directional_shadow(GAME_VIEW, PERSPECTIVE, to_light, settings);
    for (const float angle : { 0.3f, 1.2f, 2.9f, 4.4f })
    {
        const auto target = eye + glm::vec3(std::cos(angle), -1.0f, std::sin(angle));
        const auto view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
        const auto shadow = compute_directional_shadow(view, PERSPECTIVE, to_light, settings);
        EXPECT_FLOAT_EQ(shadow.world_texel_size, reference.world_texel_size);
    }
}

TEST(Shadows, ShadowMapMovesInWholeTexels)
{
    const ShadowSettings settings;
    const auto to_light = glm::vec3(0.4f, 1.0f, 0.3f);
    const auto reference = compute_directional_shadow(GAME_VIEW, PERSPECTIVE, to_light, settings);
    const auto world_point = glm::vec3(1.0f, 0.0f, -2.0f);
    const auto reference_texel = (glm::vec2(to_shadow_ndc(reference, world_point)) * 0.5f + 0.5f) * static_cast<float>(settings.map_size);

    for (const float offset : { 0.013f, 0.37f, 1.111f, 5.7f })
    {
        // camera moved, looking in the same direction
        const auto view = glm::translate(GAME_VIEW, -glm::vec3(offset, 0.0f, offset * 0.5f));
        const auto shadow = compute_directional_shadow(view, PERSPECTIVE, to_light, settings);
        const auto texel = (glm::vec2(to_shadow_ndc(shadow, world_point)) * 0.5f + 0.5f) * static_cast<float>(settings.map_size);
        const auto moved_by = texel - reference_texel;
        // a point of the world stays at the same place within a texel
        EXPECT_NEAR(moved_by.x, std::round(moved_by.x), 0.02f) << "camera offset: " << offset;
        EXPECT_NEAR(moved_by.y, std::round(moved_by.y), 0.02f) << "camera offset: " << offset;
    }
}

TEST(Shadows, DegenerateLightDirectionGivesValidMatrices)
{
    for (const auto& to_light : { glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f) })
    {
        const auto shadow = compute_directional_shadow(GAME_VIEW, PERSPECTIVE, to_light, ShadowSettings{});
        const auto ndc = to_shadow_ndc(shadow, glm::vec3(3.0f, 0.0f, 0.0f));
        EXPECT_TRUE(std::isfinite(ndc.x) && std::isfinite(ndc.y) && std::isfinite(ndc.z));
        EXPECT_GT(shadow.world_texel_size, 0.0f);
    }
}
