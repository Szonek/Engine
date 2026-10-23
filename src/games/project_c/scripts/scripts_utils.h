#pragma once
#include "engine.h"

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
// gives the game object a name component with this name (cut to its length)
void set_name(engine_game_object_t go, std::string_view name);
// the game object's position in the world, as of the last scene update (engineGameObjectGetWorldTransform); the
// origin when it has no transform
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