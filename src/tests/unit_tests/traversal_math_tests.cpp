#include "traversal_math.h"

#include <gtest/gtest.h>

#include <limits>

using namespace project_c::traversal_math;

namespace
{
constexpr float GRAVITY = 10.0f;

void expect_near(const glm::vec3& actual, const glm::vec3& expected, float tolerance = 1e-4f)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

MovePath segments(float split)
{
    MovePath path{};
    path.p0 = glm::vec3(0.0f, 0.0f, 0.0f);
    path.p1 = glm::vec3(0.0f, 2.0f, 0.0f);
    path.p2 = glm::vec3(1.0f, 2.0f, 0.0f);
    path.split = split;
    return path;
}

// height of a ballistic arc after `seconds`
float height_at(const LeapLaunch& launch, float seconds)
{
    return launch.vertical_speed * seconds - 0.5f * GRAVITY * seconds * seconds;
}
}  // namespace

TEST(TraversalMath, StraightSegmentsGoThroughTheirPointsAtTheSplit)
{
    const auto path = segments(0.7f);
    expect_near(evaluate_move_path(path, 0.0f), path.p0);
    expect_near(evaluate_move_path(path, 0.7f), path.p1);
    expect_near(evaluate_move_path(path, 1.0f), path.p2);
    // up the first segment only, then along the second only
    EXPECT_FLOAT_EQ(evaluate_move_path(path, 0.35f).x, 0.0f);
    EXPECT_FLOAT_EQ(evaluate_move_path(path, 0.85f).y, 2.0f);
    // outside [0, 1] it stays at its ends
    expect_near(evaluate_move_path(path, -1.0f), path.p0);
    expect_near(evaluate_move_path(path, 2.0f), path.p2);
}

TEST(TraversalMath, SplitOfOneIsTheFirstSegmentOnly)
{
    const auto path = segments(1.0f);
    expect_near(evaluate_move_path(path, 0.0f), path.p0);
    expect_near(evaluate_move_path(path, 1.0f), path.p1);
    EXPECT_FLOAT_EQ(evaluate_move_path(path, 0.5f).x, 0.0f);
}

TEST(TraversalMath, CurvedPathGoesThroughItsMiddlePointHalfway)
{
    auto path = segments(0.5f);
    path.curved = true;
    expect_near(evaluate_move_path(path, 0.0f), path.p0);
    expect_near(evaluate_move_path(path, 0.5f), path.p1);
    expect_near(evaluate_move_path(path, 1.0f), path.p2);
}

TEST(TraversalMath, LeapReachesItsHeightAndLandsAtItsDistance)
{
    for (const float drop : { 0.0f, 2.25f, 5.0f })
    {
        SCOPED_TRACE(drop);
        const auto launch = leap_launch(3.0f, 0.6f, drop, GRAVITY);
        ASSERT_GT(launch.horizontal_speed, 0.0f);
        // the top of the arc
        const float apex_seconds = launch.vertical_speed / GRAVITY;
        EXPECT_NEAR(height_at(launch, apex_seconds), 0.6f, 1e-4f);
        // when it has flown the distance, it is down at the landing height
        const float landing_seconds = 3.0f / launch.horizontal_speed;
        EXPECT_GT(landing_seconds, apex_seconds);
        EXPECT_NEAR(height_at(launch, landing_seconds), -drop, 1e-3f);
    }
}

TEST(TraversalMath, LongerDropsLeapSlower)
{
    // the same landing distance, further down: more time in the air
    EXPECT_LT(leap_launch(3.0f, 0.6f, 4.0f, GRAVITY).horizontal_speed, leap_launch(3.0f, 0.6f, 1.0f, GRAVITY).horizontal_speed);
}

TEST(TraversalMath, FlatLeapWithoutHeightHasNoSpeed)
{
    // no arc and nothing to fall: no time in the air to cover a distance in
    const auto launch = leap_launch(3.0f, 0.0f, 0.0f, GRAVITY);
    EXPECT_FLOAT_EQ(launch.vertical_speed, 0.0f);
    EXPECT_FLOAT_EQ(launch.horizontal_speed, 0.0f);
}

TEST(TraversalMath, AirControlKeepsTheVelocityWithoutInput)
{
    const glm::vec2 velocity(2.7f, -0.5f);
    const auto after = air_control(velocity, glm::vec2(0.0f), 2.5f, 7.2f, 0.5f);
    EXPECT_FLOAT_EQ(after.x, velocity.x);
    EXPECT_FLOAT_EQ(after.y, velocity.y);
}

TEST(TraversalMath, AirControlSteersByItsAcceleration)
{
    // from standing, toward +x: at most acceleration * dt per step
    const auto after = air_control(glm::vec2(0.0f), glm::vec2(1.0f, 0.0f), 2.5f, 6.0f, 0.1f);
    EXPECT_NEAR(after.x, 0.6f, 1e-5f);
    EXPECT_FLOAT_EQ(after.y, 0.0f);
    // close enough, it reaches the wanted velocity
    const auto reached = air_control(glm::vec2(2.4f, 0.0f), glm::vec2(1.0f, 0.0f), 2.5f, 6.0f, 0.1f);
    EXPECT_FLOAT_EQ(reached.x, 2.5f);
}

TEST(TraversalMath, AirControlDoesNotSlowALeapAlongTheInput)
{
    const glm::vec2 leap(4.0f, 0.0f);
    const auto after = air_control(leap, glm::vec2(1.0f, 0.0f), 2.5f, 6.0f, 0.1f);
    EXPECT_FLOAT_EQ(after.x, 4.0f);
}

TEST(TraversalMath, RunningIntoAWallRunsUpIt)
{
    GroundSituation wall{};
    wall.obstacle = Obstacle::eWall;
    wall.running = true;
    EXPECT_EQ(choose_ground_action(wall, 3.0f, 0.1f), GroundAction::eRunUp);
}

TEST(TraversalMath, WalkingIntoAWallClimbsItAfterTheIntentTime)
{
    GroundSituation wall{};
    wall.obstacle = Obstacle::eWall;
    wall.pushed_seconds = 0.05f;
    EXPECT_EQ(choose_ground_action(wall, 3.0f, 0.1f), GroundAction::eNone);
    wall.pushed_seconds = 0.1f;
    EXPECT_EQ(choose_ground_action(wall, 3.0f, 0.1f), GroundAction::eClimb);
}

TEST(TraversalMath, WindowsAreVaultedThrough)
{
    for (const bool running : { false, true })
    {
        GroundSituation window{};
        window.obstacle = Obstacle::eWindow;
        window.running = running;
        EXPECT_EQ(choose_ground_action(window, 3.0f, 0.1f), GroundAction::eVault);
    }
}

TEST(TraversalMath, RunningOffAnEdgeOnlyWhenTheDropIsSafe)
{
    GroundSituation edge{};
    edge.obstacle = Obstacle::eEdge;
    edge.running = true;
    edge.drop = 2.25f;
    EXPECT_EQ(choose_ground_action(edge, 3.0f, 0.1f), GroundAction::eRunOff);
    edge.drop = 3.5f;
    EXPECT_EQ(choose_ground_action(edge, 3.0f, 0.1f), GroundAction::eClimbDown);
    edge.drop = std::numeric_limits<float>::infinity();
    EXPECT_EQ(choose_ground_action(edge, 3.0f, 0.1f), GroundAction::eClimbDown);
}

TEST(TraversalMath, WalkingToAnEdgeClimbsDown)
{
    GroundSituation edge{};
    edge.obstacle = Obstacle::eEdge;
    edge.drop = 1.0f;
    EXPECT_EQ(choose_ground_action(edge, 3.0f, 0.1f), GroundAction::eClimbDown);
}

TEST(TraversalMath, NothingReachedDoesNothing)
{
    GroundSituation nothing{};
    nothing.running = true;
    EXPECT_EQ(choose_ground_action(nothing, 3.0f, 0.1f), GroundAction::eNone);
}
