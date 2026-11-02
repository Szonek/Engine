#pragma once
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

// The maths of the player's traversal (PlayerTraversal), free of the engine so it is unit tested
// (engine_logic_tests).
namespace project_c::traversal_math
{
// A scripted move's path from p0 through p1 to p2 (vault, mantle, climb down)
struct MovePath
{
    glm::vec3 p0{ 0.0f };
    glm::vec3 p1{ 0.0f };
    glm::vec3 p2{ 0.0f };
    // a quadratic curve through p1 (vault), otherwise two straight segments (mantle, climb down), each eased in and out
    bool curved = false;
    // straight segments only: the fraction of the time spent on p0 -> p1; 1 for p0 -> p1 only (p2 isn't used)
    float split = 0.5f;
};

// where the path is at `t` (0 at p0, 1 at its end)
inline glm::vec3 evaluate_move_path(const MovePath& path, float t)
{
    t = glm::clamp(t, 0.0f, 1.0f);
    if (path.curved)
    {
        // quadratic bezier passing through p1 at t = 0.5
        const glm::vec3 control = 2.0f * path.p1 - 0.5f * (path.p0 + path.p2);
        const float u = 1.0f - t;
        return u * u * path.p0 + 2.0f * u * t * control + t * t * path.p2;
    }
    if (t < path.split || path.split >= 1.0f)
    {
        return glm::mix(path.p0, path.p1, glm::smoothstep(0.0f, 1.0f, t / path.split));
    }
    return glm::mix(path.p1, path.p2, glm::smoothstep(0.0f, 1.0f, (t - path.split) / (1.0f - path.split)));
}

// the velocity a leap starts with (metres per second)
struct LeapLaunch
{
    float horizontal_speed = 0.0f;
    float vertical_speed = 0.0f;
};

// The launch of a ballistic leap rising `apex_height` above the takeoff and landing `distance` away horizontally,
// `drop` below the takeoff (0: at its height), under `gravity` (metres per second per second, positive).
inline LeapLaunch leap_launch(float distance, float apex_height, float drop, float gravity)
{
    const float g = std::max(gravity, 0.001f);
    LeapLaunch launch{};
    launch.vertical_speed = std::sqrt(2.0f * g * std::max(apex_height, 0.0f));
    // up to the apex and down to the landing height
    const float flight_seconds = (launch.vertical_speed
        + std::sqrt(launch.vertical_speed * launch.vertical_speed + 2.0f * g * std::max(drop, 0.0f))) / g;
    launch.horizontal_speed = flight_seconds > 0.0f ? std::max(distance, 0.0f) / flight_seconds : 0.0f;
    return launch;
}

// Air control: the horizontal velocity after `delta_seconds` in the air, steered toward `direction` (normalized; zero
// without input, which keeps the velocity: nothing brakes in the air) by `acceleration`. Steering never slows it below
// its speed, so a leap faster than walking keeps its speed while the input points along it.
inline glm::vec2 air_control(const glm::vec2& velocity, const glm::vec2& direction, float max_speed, float acceleration, float delta_seconds)
{
    if (glm::dot(direction, direction) <= 0.0f)
    {
        return velocity;
    }
    const glm::vec2 target = direction * std::max(max_speed, glm::length(velocity));
    const glm::vec2 delta = target - velocity;
    const float delta_length = glm::length(delta);
    const float max_change = acceleration * delta_seconds;
    return delta_length > max_change ? velocity + delta * (max_change / delta_length) : target;
}

// what the player reached on the ground, in the movement direction
enum class Obstacle
{
    eNone,
    eWall,      // a climbable wall
    eWindow,    // something to vault over
    eEdge       // the edge of a roof, with a climbable wall below
};

struct GroundSituation
{
    Obstacle obstacle = Obstacle::eNone;
    // the player approached it at running speed
    bool running = false;
    // eEdge: how far below the ground past the edge is (infinity when there is none)
    float drop = 0.0f;
    // how long the movement input has pushed into it
    float pushed_seconds = 0.0f;
};

enum class GroundAction
{
    eNone,
    eClimb,
    // up the wall at running speed (over its top when it is low enough)
    eRunUp,
    eVault,
    eClimbDown,
    // keep running: off the edge, falling
    eRunOff
};

// What the player does at what it reached: running into a wall runs up it, walking into it climbs it once the input
// has pushed into it for `climb_intent_seconds` (so brushing a wall mid-fight doesn't climb it); running at an edge runs
// off it when the drop is safe; otherwise the obstacle is traversed.
inline GroundAction choose_ground_action(const GroundSituation& situation, float max_safe_drop, float climb_intent_seconds)
{
    switch (situation.obstacle)
    {
    case Obstacle::eWall:
        if (situation.running)
        {
            return GroundAction::eRunUp;
        }
        return situation.pushed_seconds >= climb_intent_seconds ? GroundAction::eClimb : GroundAction::eNone;
    case Obstacle::eWindow:
        return GroundAction::eVault;
    case Obstacle::eEdge:
        return situation.running && situation.drop <= max_safe_drop ? GroundAction::eRunOff : GroundAction::eClimbDown;
    default:
        return GroundAction::eNone;
    }
}
}  // namespace project_c::traversal_math
