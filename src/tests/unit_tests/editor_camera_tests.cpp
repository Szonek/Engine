#include "editor_camera.h"

#include <gtest/gtest.h>

#include <glm/gtc/constants.hpp>

#include <cmath>

namespace
{
constexpr float FOV_Y = glm::radians(45.0f);
constexpr float VIEWPORT_HEIGHT = 720.0f;
constexpr float DT = 1000.0f / 60.0f;

void expect_vec_near(const glm::vec3& actual, const glm::vec3& expected, float tolerance = 1e-4f)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

// looking down -Z at the origin from 10 units away
engine::EditorCamera make_camera()
{
    return engine::EditorCamera(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f));
}

void finish_transition(engine::EditorCamera& camera)
{
    for (int i = 0; i < 60; i++)
    {
        camera.update({}, DT, VIEWPORT_HEIGHT, FOV_Y);
    }
}
}  // namespace

TEST(EditorCamera, StartsAtTheGivenEyeLookingAtThePivot)
{
    const engine::EditorCamera camera(glm::vec3(3.0f, 4.0f, 5.0f), glm::vec3(1.0f, 2.0f, -1.0f));
    expect_vec_near(camera.get_eye(), glm::vec3(3.0f, 4.0f, 5.0f));
    expect_vec_near(camera.get_pivot(), glm::vec3(1.0f, 2.0f, -1.0f));
}

TEST(EditorCamera, OrbitKeepsThePivotAndTheDistance)
{
    auto camera = make_camera();
    engine::editor_camera_input_t input{};
    input.mmb = true;
    input.mouse_delta = { 120.0f, -40.0f };
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);

    expect_vec_near(camera.get_pivot(), glm::vec3(0.0f));
    EXPECT_NEAR(glm::length(camera.get_eye()), 10.0f, 1e-4f);
    // dragging right shows the left side: the eye moves to -X
    EXPECT_LT(camera.get_eye().x, 0.0f);
    // dragging up shows it from below
    EXPECT_LT(camera.get_eye().y, 0.0f);
}

TEST(EditorCamera, AltLeftDragOrbitsLikeMiddleDrag)
{
    auto middle = make_camera();
    auto alt_left = make_camera();
    engine::editor_camera_input_t input{};
    input.mouse_delta = { 50.0f, 20.0f };
    input.mmb = true;
    middle.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    input.mmb = false;
    input.lmb = true;
    input.alt = true;
    alt_left.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    expect_vec_near(alt_left.get_eye(), middle.get_eye());
}

TEST(EditorCamera, LeftDragWithoutAltDoesNotMoveTheCamera)
{
    auto camera = make_camera();
    engine::editor_camera_input_t input{};
    input.lmb = true;
    input.mouse_delta = { 50.0f, 20.0f };
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    expect_vec_near(camera.get_eye(), glm::vec3(0.0f, 0.0f, 10.0f));
}

TEST(EditorCamera, OrbitPitchStopsShortOfStraightDown)
{
    auto camera = make_camera();
    engine::editor_camera_input_t input{};
    input.mmb = true;
    input.mouse_delta = { 0.0f, 100000.0f };
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    EXPECT_FLOAT_EQ(camera.get_pitch(), -engine::EditorCamera::MAX_PITCH);
    EXPECT_GT(camera.get_eye().y, 9.9f);
}

TEST(EditorCamera, PanMovesThePointUnderTheCursorWithIt)
{
    auto camera = make_camera();
    engine::editor_camera_input_t input{};
    input.mmb = true;
    input.shift = true;
    // half the viewport height to the right: the view at the pivot's depth is 2 * 10 * tan(fov / 2) high
    input.mouse_delta = { VIEWPORT_HEIGHT * 0.5f, 0.0f };
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);

    const float expected = 10.0f * std::tan(FOV_Y * 0.5f);
    expect_vec_near(camera.get_pivot(), glm::vec3(-expected, 0.0f, 0.0f));
    expect_vec_near(camera.get_eye(), glm::vec3(-expected, 0.0f, 10.0f));
}

TEST(EditorCamera, WheelZoomsTowardThePivotAndStopsAtTheMinimum)
{
    auto camera = make_camera();
    engine::editor_camera_input_t input{};
    input.wheel = 1.0f;
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    EXPECT_NEAR(camera.get_distance(), 10.0f * engine::EditorCamera::WHEEL_ZOOM_FACTOR, 1e-4f);
    expect_vec_near(camera.get_pivot(), glm::vec3(0.0f));

    input.wheel = 1000.0f;
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    EXPECT_FLOAT_EQ(camera.get_distance(), engine::EditorCamera::MIN_DISTANCE);

    input.wheel = -1.0f;
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    EXPECT_GT(camera.get_distance(), engine::EditorCamera::MIN_DISTANCE);
}

TEST(EditorCamera, CtrlMiddleDragZooms)
{
    auto camera = make_camera();
    engine::editor_camera_input_t input{};
    input.mmb = true;
    input.ctrl = true;
    input.mouse_delta = { 0.0f, -50.0f };
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    EXPECT_LT(camera.get_distance(), 10.0f);
    expect_vec_near(camera.get_pivot(), glm::vec3(0.0f));
}

TEST(EditorCamera, FlyMovesTheEyeAtTheFlySpeed)
{
    auto camera = make_camera();
    camera.set_fly_speed(6.0f);
    engine::editor_camera_input_t input{};
    input.rmb = true;
    input.move_forward = true;
    camera.update(input, 500.0f, VIEWPORT_HEIGHT, FOV_Y);
    EXPECT_TRUE(camera.is_flying());
    expect_vec_near(camera.get_eye(), glm::vec3(0.0f, 0.0f, 7.0f));
    // the pivot stays in front, at the same distance
    EXPECT_NEAR(camera.get_distance(), 10.0f, 1e-4f);

    input.move_forward = false;
    input.move_up = true;
    input.shift = true;
    camera.update(input, 500.0f, VIEWPORT_HEIGHT, FOV_Y);
    expect_vec_near(camera.get_eye(), glm::vec3(0.0f, 6.0f * engine::EditorCamera::FLY_BOOST * 0.5f, 7.0f));

    input = {};
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    EXPECT_FALSE(camera.is_flying());
}

TEST(EditorCamera, FlyLooksAroundTheEye)
{
    auto camera = make_camera();
    engine::editor_camera_input_t input{};
    input.rmb = true;
    input.mouse_delta = { 200.0f, 0.0f };
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    expect_vec_near(camera.get_eye(), glm::vec3(0.0f, 0.0f, 10.0f));
    // moving the mouse right looks right
    EXPECT_GT(camera.get_forward().x, 0.0f);
}

TEST(EditorCamera, WheelWhileFlyingChangesTheFlySpeedNotTheDistance)
{
    auto camera = make_camera();
    const float speed = camera.get_fly_speed();
    engine::editor_camera_input_t input{};
    input.rmb = true;
    input.wheel = 1.0f;
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    EXPECT_NEAR(camera.get_fly_speed(), speed * engine::EditorCamera::FLY_WHEEL_FACTOR, 1e-4f);
    EXPECT_NEAR(camera.get_distance(), 10.0f, 1e-4f);
}

TEST(EditorCamera, FrameGlidesOntoTheSphere)
{
    auto camera = make_camera();
    camera.frame(glm::vec3(5.0f, 1.0f, -3.0f), 2.0f, FOV_Y);
    // nothing jumps on the frame it starts
    expect_vec_near(camera.get_pivot(), glm::vec3(0.0f));

    finish_transition(camera);
    expect_vec_near(camera.get_pivot(), glm::vec3(5.0f, 1.0f, -3.0f));
    EXPECT_NEAR(camera.get_distance(), 2.0f / std::sin(FOV_Y * 0.5f), 1e-3f);
    // keeps looking the same way
    expect_vec_near(camera.get_forward(), glm::vec3(0.0f, 0.0f, -1.0f));
}

TEST(EditorCamera, NavigatingStopsAGlide)
{
    auto camera = make_camera();
    camera.frame(glm::vec3(5.0f, 0.0f, 0.0f), 1.0f, FOV_Y);
    camera.update({}, DT, VIEWPORT_HEIGHT, FOV_Y);
    const auto pivot = camera.get_pivot();

    engine::editor_camera_input_t input{};
    input.mmb = true;
    camera.update(input, DT, VIEWPORT_HEIGHT, FOV_Y);
    finish_transition(camera);
    expect_vec_near(camera.get_pivot(), pivot);
}

TEST(EditorCamera, SetViewLooksAlongTheAxis)
{
    auto camera = make_camera();
    // right view: from +X
    camera.set_view(-glm::half_pi<float>(), 0.0f);
    finish_transition(camera);
    expect_vec_near(camera.get_forward(), glm::vec3(-1.0f, 0.0f, 0.0f));
    expect_vec_near(camera.get_eye(), glm::vec3(10.0f, 0.0f, 0.0f));

    // back view: turns the short way round (through +Z), not back through the front view
    camera.set_view(glm::pi<float>(), 0.0f);
    camera.update({}, DT, VIEWPORT_HEIGHT, FOV_Y);
    EXPECT_GT(camera.get_forward().z, 0.0f);
    finish_transition(camera);
    expect_vec_near(camera.get_forward(), glm::vec3(0.0f, 0.0f, 1.0f));
}
