#pragma once
#include "engine.h"

#include <vector>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>


namespace project_c
{
namespace utils
{
void delete_game_objects_hierarchy(engine_scene_t scene, engine_game_object_t go);
std::vector<engine_game_object_t> get_active_camera_game_objects(engine_scene_t scene);
std::vector<engine_game_object_t> get_game_objects_with_name(engine_scene_t scene, std::string_view name);
glm::quat rotate_toward(glm::vec3 origin, glm::vec3 target);
engine_ray_t get_ray_from_mouse_position(engine_application_t app, engine_scene_t scene, engine_game_object_t go_camera);

} // namespace utils
} // namespace project