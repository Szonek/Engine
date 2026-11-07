#pragma once
#include "engine.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string_view>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace project_c
{
namespace utils
{
// the game objects whose parent is `go`; a copy, so they can be destroyed or reparented while iterating
std::vector<engine_game_object_t> get_children(engine_game_object_t go);
// Copies `text` into a fixed char array field (a component's string), cut to fit with its terminating zero; false
// when it was cut.
template<std::size_t N>
bool copy_string(char (&field)[N], std::string_view text)
{
    const auto length = std::min(text.size(), N - 1);
    std::memcpy(field, text.data(), length);
    field[length] = '\0';
    return length == text.size();
}
// The text of a fixed char array field. A field loaded from a file can fill the array with no terminating zero, so
// it never reads past its end.
template<std::size_t N>
std::string_view to_string_view(const char (&field)[N])
{
    return std::string_view(field, std::find(field, field + N, '\0') - field);
}
// gives the game object a name component with this name (cut to its length)
void set_name(engine_game_object_t go, std::string_view name);
// The game object's position in the world from its transform and its parents' (joint attachments aren't followed), so
// it is current also before this frame's scene update, unlike engineGameObjectGetWorldTransform; the origin when it
// has no transform.
glm::vec3 get_world_position(engine_game_object_t go);
std::vector<engine_game_object_t> get_active_camera_game_objects();
glm::quat rotate_toward(glm::vec3 origin, glm::vec3 target);
struct CursorRay
{
    glm::vec3 origin;     // the point under the cursor on the camera's near plane
    glm::vec3 direction;  // to the point under the cursor on the far plane (not normalized)
};
// ray through the cursor (PlayerInput::cursor) from the near to the far plane of the camera
CursorRay get_ray_from_cursor(engine_game_object_t go_camera, engine_fvec2_t cursor);

// Whether level geometry (a static or kinematic body) is on the segment from `from` to `to`: characters and dynamic
// bodies (other enemies, a dropped weapon) don't count, the ray is cast again past them, up to 4 of them (more count as
// blocking). `ignore` are skipped too (the ones looking and looked at).
bool level_geometry_between(const glm::vec3& from, const glm::vec3& to, std::span<const engine_game_object_t> ignore);
// The first level geometry hit by the ray from `origin` along `direction` within `max_distance`, skipping characters
// and dynamic bodies like level_geometry_between (and `ignore`, at most MAX_LEVEL_RAY_IGNORED of them); none when it
// reaches nothing (or only more than 4 characters and dynamic bodies).
inline constexpr std::size_t MAX_LEVEL_RAY_IGNORED = 4;
std::optional<engine_ray_hit_t> level_geometry_ray(const glm::vec3& origin, const glm::vec3& direction, float max_distance,
    std::span<const engine_game_object_t> ignore);
} // namespace utils
} // namespace project_c