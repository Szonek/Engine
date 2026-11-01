#pragma once
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace engine
{
struct DebugDrawLine
{
    glm::vec3 from{};
    glm::vec3 to{};
    glm::vec3 color{ 1.0f };
    // the game object the line was drawn for (a gizmo's), which the editor selects when it is clicked; 0 (the invalid
    // game object) for none
    std::uint32_t owner = 0;
};

// Immediate mode debug shapes of a scene (engineDebugDraw*, like Unreal's DrawDebugLine / DrawDebugBox): in world
// space, made of lines, for one frame only. What is added during a frame is taken by that frame's scene update, which
// draws it with the physics and nav mesh debug lines, and the list starts empty again. Knows shapes, never game concepts.
// Shapes can have an owner (like Unreal's hit proxies): the game object the editor selects when one of their lines is
// clicked.
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
    // the sphere as its three great circles around the world axes (like Unity's Gizmos.DrawWireSphere)
    void add_sphere(const glm::vec3& center, float radius, const glm::vec3& color);

    // the owner of the shapes added after it (0 for none), until it is set again or the lines are taken
    void set_owner(std::uint32_t owner) { owner_ = owner; }

    const std::vector<DebugDrawLine>& lines() const { return lines_; }
    // the lines added since the last take, leaving the list empty and without an owner; their owners are kept as the
    // last owners
    std::vector<DebugDrawLine> take_lines();
    // the owners of the lines last taken (what is on the screen), sorted, each once
    const std::vector<std::uint32_t>& last_owners() const { return last_owners_; }

private:
    std::vector<DebugDrawLine> lines_;
    std::uint32_t owner_ = 0;
    std::vector<std::uint32_t> last_owners_;
};

// What a click picks from the game object IDs of the pixels around it (`ids`, `width` x `height` row by row, the click
// at (center_x, center_y)): debug lines are a pixel wide, so the nearest pixel of an owner of debug lines (`owners`,
// sorted) within the area wins, like a gizmo in Unity or Unreal; without one, the pixel under the click. 0 for nothing.
std::uint32_t pick_game_object(std::span<const std::uint32_t> ids, std::uint32_t width, std::uint32_t height,
    std::uint32_t center_x, std::uint32_t center_y, std::span<const std::uint32_t> owners);
}  // namespace engine
