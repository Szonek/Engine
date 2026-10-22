#include "editor_modal_transform.h"

#include <gtest/gtest.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace
{
constexpr glm::vec2 VIEWPORT{ 800.0f, 600.0f };
constexpr glm::vec2 CENTER{ 400.0f, 300.0f };

// looking down -Z at the origin from 10 units away
engine::modal_transform_view_t make_view()
{
    engine::modal_transform_view_t view{};
    view.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    view.projection = glm::perspective(glm::radians(45.0f), VIEWPORT.x / VIEWPORT.y, 0.1f, 100.0f);
    view.viewport_size = VIEWPORT;
    return view;
}

glm::vec2 project(const glm::vec3& world)
{
    const auto view = make_view();
    const auto clip = view.projection * view.view * glm::vec4(world, 1.0f);
    const auto ndc = glm::vec2(clip) / clip.w;
    return { (ndc.x + 1.0f) * 0.5f * VIEWPORT.x, (1.0f - ndc.y) * 0.5f * VIEWPORT.y };
}

void expect_vec_near(const glm::vec3& actual, const glm::vec3& expected, float tolerance = 1e-3f)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

glm::vec3 origin_of(const glm::mat4& m)
{
    return glm::vec3(m[3]);
}
}  // namespace

using engine::ModalTransform;
using engine::ModalTransformAxis;
using engine::ModalTransformMode;

TEST(ModalTransform, GrabKeepsTheObjectUnderTheCursor)
{
    ModalTransform transform(ModalTransformMode::eGrab, glm::mat4(1.0f), CENTER, make_view());
    const auto result = transform.update(CENTER + glm::vec2(120.0f, -80.0f), false);
    const auto on_screen = project(origin_of(result));
    EXPECT_NEAR(on_screen.x, CENTER.x + 120.0f, 0.1f);
    EXPECT_NEAR(on_screen.y, CENTER.y - 80.0f, 0.1f);
    // at the same depth
    EXPECT_NEAR(origin_of(result).z, 0.0f, 1e-3f);
}

TEST(ModalTransform, GrabStartsWhereverTheCursorIs)
{
    // the cursor needn't be over the object: only its motion counts
    ModalTransform transform(ModalTransformMode::eGrab, glm::mat4(1.0f), glm::vec2(100.0f, 100.0f), make_view());
    expect_vec_near(origin_of(transform.update(glm::vec2(100.0f, 100.0f), false)), glm::vec3(0.0f));
}

TEST(ModalTransform, GrabAlongAnAxisMovesOnlyAlongIt)
{
    ModalTransform transform(ModalTransformMode::eGrab, glm::mat4(1.0f), CENTER, make_view());
    transform.toggle_axis(ModalTransformAxis::eX);
    const auto result = transform.update(CENTER + glm::vec2(100.0f, 60.0f), false);
    const auto origin = origin_of(result);
    EXPECT_GT(origin.x, 0.0f);
    EXPECT_NEAR(origin.y, 0.0f, 1e-4f);
    EXPECT_NEAR(origin.z, 0.0f, 1e-4f);
    // the point of the axis under the cursor
    EXPECT_NEAR(project(origin_of(transform.update(CENTER + glm::vec2(100.0f, 0.0f), false))).x, CENTER.x + 100.0f, 0.1f);
}

TEST(ModalTransform, GrabAlongTheAxisPointingAtTheCameraDoesNotJump)
{
    ModalTransform transform(ModalTransformMode::eGrab, glm::mat4(1.0f), CENTER, make_view());
    transform.toggle_axis(ModalTransformAxis::eZ);
    expect_vec_near(origin_of(transform.update(CENTER + glm::vec2(1.0f, 0.0f), false)), glm::vec3(0.0f));
}

TEST(ModalTransform, GrabSnapsToSteps)
{
    ModalTransform transform(ModalTransformMode::eGrab, glm::mat4(1.0f), CENTER, make_view());
    const auto origin = origin_of(transform.update(CENTER + glm::vec2(77.0f, 31.0f), true));
    for (int i = 0; i < 3; i++)
    {
        EXPECT_NEAR(std::remainder(origin[i], ModalTransform::GRAB_SNAP), 0.0f, 1e-4f);
    }
    EXPECT_GT(origin.x, 0.0f);
}

TEST(ModalTransform, LocalAxisFollowsTheObjectsRotation)
{
    // turned a quarter about Z: its X axis is the world's Y
    const auto world = glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
    ModalTransform transform(ModalTransformMode::eGrab, world, CENTER, make_view());
    transform.toggle_axis(ModalTransformAxis::eX);
    EXPECT_FALSE(transform.is_local());
    transform.toggle_axis(ModalTransformAxis::eX);
    EXPECT_TRUE(transform.is_local());
    expect_vec_near(transform.get_axis_direction(), glm::vec3(0.0f, 1.0f, 0.0f));
    const auto origin = origin_of(transform.update(CENTER + glm::vec2(0.0f, -100.0f), false));
    EXPECT_GT(origin.y, 0.0f);
    EXPECT_NEAR(origin.x, 0.0f, 1e-4f);
}

TEST(ModalTransform, AxisKeysCycleGlobalLocalNone)
{
    ModalTransform transform(ModalTransformMode::eGrab, glm::mat4(1.0f), CENTER, make_view());
    transform.toggle_axis(ModalTransformAxis::eY);
    EXPECT_EQ(transform.get_axis(), ModalTransformAxis::eY);
    EXPECT_FALSE(transform.is_local());
    transform.toggle_axis(ModalTransformAxis::eY);
    EXPECT_TRUE(transform.is_local());
    transform.toggle_axis(ModalTransformAxis::eY);
    EXPECT_EQ(transform.get_axis(), ModalTransformAxis::eNone);
    // another axis starts global again
    transform.toggle_axis(ModalTransformAxis::eY);
    transform.toggle_axis(ModalTransformAxis::eY);
    transform.toggle_axis(ModalTransformAxis::eZ);
    EXPECT_EQ(transform.get_axis(), ModalTransformAxis::eZ);
    EXPECT_FALSE(transform.is_local());
}

TEST(ModalTransform, ScaleAxesAreLocal)
{
    ModalTransform transform(ModalTransformMode::eScale, glm::mat4(1.0f), CENTER + glm::vec2(100.0f, 0.0f), make_view());
    transform.toggle_axis(ModalTransformAxis::eX);
    EXPECT_TRUE(transform.is_local());
    transform.toggle_axis(ModalTransformAxis::eX);
    EXPECT_EQ(transform.get_axis(), ModalTransformAxis::eNone);
}

TEST(ModalTransform, RotateFollowsTheCursorAroundTheObject)
{
    // from the right of the object to above it: a quarter turn counterclockwise, about the axis toward the viewer
    ModalTransform transform(ModalTransformMode::eRotate, glm::mat4(1.0f), CENTER + glm::vec2(100.0f, 0.0f), make_view());
    const auto result = transform.update(CENTER + glm::vec2(0.0f, -100.0f), false);
    EXPECT_NEAR(transform.get_value().x, 90.0f, 1e-2f);
    expect_vec_near(glm::vec3(result[0]), glm::vec3(0.0f, 1.0f, 0.0f));
    expect_vec_near(origin_of(result), glm::vec3(0.0f));
}

TEST(ModalTransform, RotateAboutItsOwnOrigin)
{
    const auto world = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 0.0f));
    const auto center = project(glm::vec3(2.0f, 1.0f, 0.0f));
    ModalTransform transform(ModalTransformMode::eRotate, world, center + glm::vec2(100.0f, 0.0f), make_view());
    transform.toggle_axis(ModalTransformAxis::eY);
    const auto result = transform.update(center + glm::vec2(0.0f, -100.0f), false);
    expect_vec_near(origin_of(result), glm::vec3(2.0f, 1.0f, 0.0f));
    // about Y: X turns within the XZ plane
    EXPECT_NEAR(result[0].y, 0.0f, 1e-4f);
    EXPECT_NEAR(std::abs(result[0].z), 1.0f, 1e-3f);
}

TEST(ModalTransform, RotateGoesPastHalfATurn)
{
    ModalTransform transform(ModalTransformMode::eRotate, glm::mat4(1.0f), CENTER + glm::vec2(100.0f, 0.0f), make_view());
    for (int i = 1; i <= 36; i++)
    {
        const float angle = glm::radians(10.0f * static_cast<float>(i));
        transform.update(CENTER + 100.0f * glm::vec2(std::cos(angle), -std::sin(angle)), false);
    }
    EXPECT_NEAR(transform.get_value().x, 360.0f, 1e-2f);
}

TEST(ModalTransform, RotateSnapsToSteps)
{
    ModalTransform transform(ModalTransformMode::eRotate, glm::mat4(1.0f), CENTER + glm::vec2(100.0f, 0.0f), make_view());
    const float angle = glm::radians(23.0f);
    transform.update(CENTER + 100.0f * glm::vec2(std::cos(angle), -std::sin(angle)), true);
    EXPECT_NEAR(transform.get_value().x, 25.0f, 1e-3f);
}

TEST(ModalTransform, ScaleByTheCursorsDistanceFromTheObject)
{
    ModalTransform transform(ModalTransformMode::eScale, glm::mat4(1.0f), CENTER + glm::vec2(100.0f, 0.0f), make_view());
    auto result = transform.update(CENTER + glm::vec2(0.0f, 200.0f), false);
    EXPECT_NEAR(transform.get_value().x, 2.0f, 1e-3f);
    EXPECT_NEAR(glm::length(glm::vec3(result[0])), 2.0f, 1e-3f);
    EXPECT_NEAR(glm::length(glm::vec3(result[1])), 2.0f, 1e-3f);

    transform.toggle_axis(ModalTransformAxis::eX);
    result = transform.update(CENTER + glm::vec2(150.0f, 0.0f), false);
    EXPECT_NEAR(glm::length(glm::vec3(result[0])), 1.5f, 1e-3f);
    EXPECT_NEAR(glm::length(glm::vec3(result[1])), 1.0f, 1e-3f);
}

TEST(ModalTransform, ChangingModeStartsFromTheCurrentResult)
{
    ModalTransform transform(ModalTransformMode::eGrab, glm::mat4(1.0f), CENTER, make_view());
    const auto moved = origin_of(transform.update(CENTER + glm::vec2(100.0f, 0.0f), false));
    // away from the moved object, which is under the cursor now
    const auto mouse = CENTER + glm::vec2(200.0f, 0.0f);
    transform.set_mode(ModalTransformMode::eScale, mouse);
    expect_vec_near(origin_of(transform.update(mouse, false)), moved);
    EXPECT_NEAR(glm::length(glm::vec3(transform.get_result()[0])), 1.0f, 1e-3f);
}
