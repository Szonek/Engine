#include "render/pixel_art.h"

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <initializer_list>

namespace
{
using namespace engine;

// window uv (0..1) of a world point
glm::vec2 to_uv(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& world)
{
    const auto clip = projection * view * glm::vec4(world, 1.0f);
    return glm::vec2(clip) / clip.w * 0.5f + 0.5f;
}

// like the game camera: orthographic, looking down at 45 degrees
glm::mat4 game_view(const glm::vec3& target)
{
    const auto eye = target + 20.0f * glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f));
    return glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 game_projection(glm::uvec2 window)
{
    const float aspect = static_cast<float>(window.x) / static_cast<float>(window.y);
    constexpr float half_height = 5.0f;
    return glm::ortho(-aspect * half_height, aspect * half_height, -half_height, half_height, 0.1f, 100.0f);
}

const std::initializer_list<glm::vec3> WORLD_POINTS = {
    { 0.0f, 0.0f, 0.0f }, { 1.3f, 0.7f, -2.1f }, { -4.2f, 1.9f, 0.4f }, { 2.6f, -0.3f, 3.3f } };

TEST(PixelArt, PixelScaleFromTargetHeight)
{
    // the same 360 lines on the common screens
    EXPECT_EQ(compute_pixel_scale(1080, 360), 3u);
    EXPECT_EQ(compute_pixel_scale(1440, 360), 4u);
    EXPECT_EQ(compute_pixel_scale(2160, 360), 6u);
    EXPECT_EQ(compute_pixel_scale(720, 360), 2u);
    // closest whole number
    EXPECT_EQ(compute_pixel_scale(1200, 360), 3u);
    EXPECT_EQ(compute_pixel_scale(1300, 360), 4u);
    // off, window smaller than the target, limit
    EXPECT_EQ(compute_pixel_scale(1080, 0), 1u);
    EXPECT_EQ(compute_pixel_scale(180, 360), 1u);
    EXPECT_EQ(compute_pixel_scale(4320, 100), PIXEL_ART_MAX_SCALE);
}

TEST(PixelArt, SceneTargetSize)
{
    EXPECT_EQ(compute_scene_target_size({ 1920, 1080 }, 1), glm::uvec2(1920, 1080));
    EXPECT_EQ(compute_scene_target_size({ 1920, 1080 }, 3), glm::uvec2(640 + 2 * PIXEL_ART_MARGIN, 360 + 2 * PIXEL_ART_MARGIN));
    // rounded up
    EXPECT_EQ(compute_scene_target_size({ 1000, 500 }, 3), glm::uvec2(334 + 2 * PIXEL_ART_MARGIN, 167 + 2 * PIXEL_ART_MARGIN));
}

TEST(PixelArt, NativeResolutionChangesNothing)
{
    const glm::uvec2 window(1280, 720);
    const auto view = game_view(glm::vec3(0.37f, 0.0f, 1.21f));
    const auto projection = game_projection(window);
    const auto camera = compute_low_res_camera(view, projection, window, 1, true);
    EXPECT_EQ(camera.view, view);
    EXPECT_EQ(camera.projection, projection);
    EXPECT_EQ(camera.present.scale, glm::vec2(1.0f));
    EXPECT_EQ(camera.present.offset, glm::vec2(0.0f));
}

// A world point is shown at the same window position as with the original camera: window uv mapped by present
// is where the low resolution camera draws it.
TEST(PixelArt, PresentShowsWhatTheOriginalCameraSees)
{
    const glm::uvec2 window(1280, 720);
    for (const bool snap : { false, true })
    {
        for (const auto& target : { glm::vec3(0.0f), glm::vec3(0.37f, 0.0f, 1.21f), glm::vec3(-3.71f, 0.5f, 7.03f) })
        {
            const auto view = game_view(target);
            const auto projection = game_projection(window);
            const auto camera = compute_low_res_camera(view, projection, window, 3, snap);
            for (const auto& world : WORLD_POINTS)
            {
                const auto window_uv = to_uv(view, projection, world);
                const auto target_uv = to_uv(camera.view, camera.projection, world);
                const auto presented_uv = window_uv * camera.present.scale + camera.present.offset;
                EXPECT_NEAR(presented_uv.x, target_uv.x, 1e-4f) << "snap " << snap;
                EXPECT_NEAR(presented_uv.y, target_uv.y, 1e-4f) << "snap " << snap;
            }
        }
    }
}

// Snapped: a world point stays at the same position within its texel however the camera moves,
// the image is only shifted by less than a texel when presented.
TEST(PixelArt, SnappedTexelGridIsFixedInTheWorld)
{
    const glm::uvec2 window(1280, 720);
    const auto target_size = glm::vec2(compute_scene_target_size(window, 3));
    const auto projection = game_projection(window);
    const auto texel_fraction = [&](const glm::vec3& camera_target, const glm::vec3& world)
    {
        const auto camera = compute_low_res_camera(game_view(camera_target), projection, window, 3, true);
        const auto texel = to_uv(camera.view, camera.projection, world) * target_size;
        return texel - glm::floor(texel);
    };
    for (const auto& world : WORLD_POINTS)
    {
        const auto reference = texel_fraction(glm::vec3(0.0f), world);
        for (const auto& camera_target : { glm::vec3(0.013f, 0.0f, 0.0f), glm::vec3(0.37f, 0.0f, 1.21f), glm::vec3(-3.71f, 0.5f, 7.03f) })
        {
            const auto difference = glm::abs(texel_fraction(camera_target, world) - reference);
            // fractions near 0 and near 1 are the same position
            const auto distance = glm::min(difference, 1.0f - difference);
            EXPECT_LT(distance.x, 2e-3f);
            EXPECT_LT(distance.y, 2e-3f);
        }
    }
}

TEST(PixelArt, SnapShiftsLessThanTheMargin)
{
    const glm::uvec2 window(1280, 720);
    const auto target_size = glm::vec2(compute_scene_target_size(window, 3));
    const auto projection = game_projection(window);
    for (float x = 0.0f; x < 1.0f; x += 0.0173f)
    {
        const auto camera = compute_low_res_camera(game_view(glm::vec3(x, 0.0f, x * 0.5f)), projection, window, 3, true);
        const auto unsnapped_offset = 0.5f - 0.5f * camera.present.scale;
        const auto shift_in_texels = (camera.present.offset - unsnapped_offset) * target_size;
        EXPECT_LE(std::abs(shift_in_texels.x), 0.5f + 1e-3f);
        EXPECT_LE(std::abs(shift_in_texels.y), 0.5f + 1e-3f);
        EXPECT_LE(0.5f, static_cast<float>(PIXEL_ART_MARGIN));
    }
}

// A moving object, snapped by its origin, is at the same position within its texel wherever it is and wherever the
// camera is: it's rasterized the same way every frame (no flickering edges), only moved by whole texels.
TEST(PixelArt, SnappedObjectsKeepTheirPlaceInTheTexel)
{
    const glm::uvec2 window(1280, 720);
    const auto target_size = glm::vec2(compute_scene_target_size(window, 3));
    const auto projection = game_projection(window);
    const glm::vec3 vertex_offset(0.31f, 1.17f, -0.23f);  // a vertex of the object, relative to its origin
    const auto vertex_texel_fraction = [&](const glm::vec3& camera_target, const glm::vec3& object_origin)
    {
        const auto camera = compute_low_res_camera(game_view(camera_target), projection, window, 3, true);
        const auto vertex = object_origin + compute_object_snap_offset(camera, object_origin) + vertex_offset;
        const auto texel = to_uv(camera.view, camera.projection, vertex) * target_size;
        return texel - glm::floor(texel);
    };
    const auto reference = vertex_texel_fraction(glm::vec3(0.0f), glm::vec3(0.0f));
    for (float t = 0.0f; t < 2.0f; t += 0.037f)
    {
        // the camera follows the object, like the game camera follows the player
        const glm::vec3 object_origin(t, 0.0f, 0.4f * t);
        const glm::vec3 camera_target = object_origin * 0.93f;
        const auto difference = glm::abs(vertex_texel_fraction(camera_target, object_origin) - reference);
        const auto distance = glm::min(difference, 1.0f - difference);
        EXPECT_LT(distance.x, 2e-3f) << "t " << t;
        EXPECT_LT(distance.y, 2e-3f) << "t " << t;
    }
}

TEST(PixelArt, ObjectsAreMovedHorizontallyByLessThanHalfATexelOnTheScreen)
{
    const glm::uvec2 window(1280, 720);
    const auto camera = compute_low_res_camera(game_view(glm::vec3(0.37f, 0.0f, 1.21f)), game_projection(window), window, 3, true);
    for (const auto& world : WORLD_POINTS)
    {
        const auto offset = compute_object_snap_offset(camera, world);
        EXPECT_EQ(offset.y, 0.0f);  // height doesn't change
        const auto view_offset = glm::mat3(camera.view) * offset;
        EXPECT_LE(std::abs(view_offset.x), 0.5f * camera.texel_size.x + 1e-5f);
        EXPECT_LE(std::abs(view_offset.y), 0.5f * camera.texel_size.y + 1e-5f);
    }
    // nothing to snap to without a snapped camera
    const auto unsnapped = compute_low_res_camera(game_view(glm::vec3(0.37f, 0.0f, 1.21f)), game_projection(window), window, 3, false);
    EXPECT_EQ(compute_object_snap_offset(unsnapped, glm::vec3(1.3f, 0.7f, -2.1f)), glm::vec3(0.0f));
}

// The shadow of a snapped object on the ground moves in whole texels with it: no jitter while the object moves.
TEST(PixelArt, ShadowOfASnappedObjectKeepsItsPlaceInTheTexel)
{
    const glm::uvec2 window(1280, 720);
    const auto target_size = glm::vec2(compute_scene_target_size(window, 3));
    const auto camera = compute_low_res_camera(game_view(glm::vec3(0.0f)), game_projection(window), window, 3, true);
    const auto direction_to_light = glm::normalize(glm::vec3(0.4f, 1.0f, 0.3f));
    const glm::vec3 shoulder(0.2f, 1.4f, 0.1f);  // a point of the object, relative to its origin
    const auto shadow_texel_fraction = [&](const glm::vec3& object_origin)
    {
        const auto point = object_origin + compute_object_snap_offset(camera, object_origin) + shoulder;
        // where the point's shadow falls on the ground plane (y = 0)
        const auto shadow = point - direction_to_light * (point.y / direction_to_light.y);
        const auto texel = to_uv(camera.view, camera.projection, shadow) * target_size;
        return texel - glm::floor(texel);
    };
    const auto reference = shadow_texel_fraction(glm::vec3(0.0f));
    for (float t = 0.0f; t < 2.0f; t += 0.037f)
    {
        const auto difference = glm::abs(shadow_texel_fraction(glm::vec3(t, 0.0f, 0.4f * t)) - reference);
        const auto distance = glm::min(difference, 1.0f - difference);
        EXPECT_LT(distance.x, 2e-3f) << "t " << t;
        EXPECT_LT(distance.y, 2e-3f) << "t " << t;
    }
}

TEST(PixelArt, PerspectiveCameraIsNotSnapped)
{
    const glm::uvec2 window(1280, 720);
    const auto view = game_view(glm::vec3(0.37f, 0.0f, 1.21f));
    const auto projection = glm::perspective(glm::radians(60.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
    const auto camera = compute_low_res_camera(view, projection, window, 3, true);
    EXPECT_EQ(camera.view, view);
    EXPECT_EQ(camera.present.offset, 0.5f - 0.5f * camera.present.scale);
}

}  // namespace
