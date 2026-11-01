#include "editor_context_click.h"

#include <gtest/gtest.h>

namespace
{
engine::editor_camera_input_t rmb(glm::vec2 mouse_delta = glm::vec2(0.0f))
{
    engine::editor_camera_input_t input{};
    input.rmb = true;
    input.mouse_delta = mouse_delta;
    return input;
}

// presses and holds the right button for the inputs, then releases it: whether the release was a click
bool click(engine::ContextClick& detector, std::initializer_list<engine::editor_camera_input_t> held, bool over_scene = true)
{
    for (const auto& input : held)
    {
        EXPECT_FALSE(detector.update(input, over_scene));
    }
    return detector.update(engine::editor_camera_input_t{}, over_scene);
}
}  // namespace

TEST(EditorContextClick, ReleaseWithoutMovingIsAClick)
{
    engine::ContextClick detector;
    EXPECT_TRUE(click(detector, { rmb(), rmb() }));
    // only once: nothing is held any more
    EXPECT_FALSE(detector.update(engine::editor_camera_input_t{}, true));
}

TEST(EditorContextClick, SmallMovesAreStillAClick)
{
    engine::ContextClick detector;
    EXPECT_TRUE(click(detector, { rmb(), rmb(glm::vec2(2.0f, 0.0f)), rmb(glm::vec2(0.0f, -3.0f)) }));
}

TEST(EditorContextClick, MovingAlongAPathLongerThanTheThresholdFlies)
{
    engine::ContextClick detector;
    // back where it started, but the camera turned on the way
    EXPECT_FALSE(click(detector, { rmb(), rmb(glm::vec2(5.0f, 0.0f)), rmb(glm::vec2(-5.0f, 0.0f)) }));
}

TEST(EditorContextClick, FlyKeysAndTheWheelFly)
{
    engine::ContextClick detector;
    auto forward = rmb();
    forward.move_forward = true;
    EXPECT_FALSE(click(detector, { rmb(), forward, rmb() }));

    auto up = rmb();
    up.move_up = true;
    EXPECT_FALSE(click(detector, { up }));

    auto wheel = rmb();
    wheel.wheel = 1.0f;
    EXPECT_FALSE(click(detector, { wheel }));
}

TEST(EditorContextClick, OtherButtonsAndAltAreNoClick)
{
    engine::ContextClick detector;
    auto with_left = rmb();
    with_left.lmb = true;
    EXPECT_FALSE(click(detector, { rmb(), with_left }));

    auto alt = rmb();
    alt.alt = true;
    EXPECT_FALSE(click(detector, { alt }));
}

TEST(EditorContextClick, APressOutsideTheSceneIsNoClick)
{
    engine::ContextClick detector;
    EXPECT_FALSE(click(detector, { rmb() }, false));
    // the next press over the scene is one again
    EXPECT_TRUE(click(detector, { rmb() }));
}
