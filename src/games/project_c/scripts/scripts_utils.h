#pragma once
#include "engine.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>
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

} // namespace utils
} // namespace project_c