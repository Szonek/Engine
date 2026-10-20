#pragma once
#include "engine.h"

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace project_c
{
namespace utils
{
// the game objects whose parent is `go`; a copy, so they can be destroyed or reparented while iterating
std::vector<engine_game_object_t> get_children(engine_game_object_t go);
std::vector<engine_game_object_t> get_active_camera_game_objects();
glm::quat rotate_toward(glm::vec3 origin, glm::vec3 target);
// ray through the cursor (PlayerInput::cursor) from the near to the far plane of the camera
engine_ray_t get_ray_from_cursor(engine_game_object_t go_camera, engine_fvec2_t cursor);

} // namespace utils
} // namespace project_c