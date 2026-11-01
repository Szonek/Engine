#pragma once
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine
{
struct DebugDrawLine
{
    glm::vec3 from{};
    glm::vec3 to{};
    glm::vec3 color{ 1.0f };
};

// Immediate mode debug shapes of a scene (engineDebugDraw*, like Unreal's DrawDebugLine / DrawDebugBox): in world
// space, made of lines, for one frame only. What is added during a frame is taken by that frame's scene update, which
// draws it with the physics and nav mesh debug lines, and the list starts empty again. Knows shapes, never game concepts.
class DebugDrawList
{
public:
    static constexpr std::uint32_t CIRCLE_SEGMENTS = 32;

    void add_line(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color);
    // the box from -half_extents to half_extents in the space of `transform` (a world matrix: rotated, scaled, moved),
    // as its 12 edges
    void add_box(const glm::mat4& transform, const glm::vec3& half_extents, const glm::vec3& color);
    // the circle around `center` in the plane `normal` is perpendicular to, as CIRCLE_SEGMENTS lines; nothing when the
    // normal has no length
    void add_circle(const glm::vec3& center, const glm::vec3& normal, float radius, const glm::vec3& color);

    const std::vector<DebugDrawLine>& lines() const { return lines_; }
    // the lines added since the last take, leaving the list empty
    std::vector<DebugDrawLine> take_lines();

private:
    std::vector<DebugDrawLine> lines_;
};
}  // namespace engine
