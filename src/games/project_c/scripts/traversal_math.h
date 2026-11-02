#pragma once
#include <glm/glm.hpp>

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
}  // namespace project_c::traversal_math
