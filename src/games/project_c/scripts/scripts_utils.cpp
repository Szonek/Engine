#include <profiler.h>
#include "scripts_utils.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>

void project_c::utils::delete_game_objects_hierarchy(engine_scene_t scene, engine_game_object_t go)
{
    if (engineSceneHasChildrenComponent(scene, go))
    {
        auto cc = engineSceneGetChildrenComponent(scene, go);
        for (auto i = 0; i < std::size(cc.child); i++)
        {
            if (cc.child[i] != ENGINE_INVALID_GAME_OBJECT_ID)
            {
                delete_game_objects_hierarchy(scene, cc.child[i]);
                engineSceneDestroyGameObject(scene, cc.child[i]);
            }
        }
    }
}

std::vector<engine_game_object_t> project_c::utils::get_active_camera_game_objects(engine_scene_t scene)
{
    engine::ScopedProfiler prof("project_c::utils::get_active_camera_game_objects");
    engine_component_view_t cv{};
    engineCreateComponentView(&cv);
    engineSceneComponentViewAttachCameraComponent(scene, cv);

    engine_component_iterator_t begin{};
    engine_component_iterator_t end{};
    engineComponentViewCreateBeginComponentIterator(cv, &begin);
    engineComponentViewCreateEndComponentIterator(cv, &end);

    std::vector<engine_game_object_t> ret{};
    while (!engineComponentIteratorCheckEqual(begin, end))
    {
        auto go_it = engineComponentIteratorGetGameObject(begin);
        if (engineSceneHasCameraComponent(scene, go_it))
        {
            if (engineSceneGetCameraComponent(scene, go_it).enabled)
            {
                ret.push_back(go_it);
            }
        }
        engineComponentIteratorNext(begin);
    }
    engineDestroyComponentView(cv);
    return ret;
}
std::vector<engine_game_object_t> project_c::utils::get_game_objects_with_name(engine_scene_t scene, std::string_view name)
{
    engine::ScopedProfiler prof("project_c::utils::get_game_objects_with_name");
    engine_component_view_t cv{};
    engineCreateComponentView(&cv);
    engineSceneComponentViewAttachNameComponent(scene, cv);

    engine_component_iterator_t begin{};
    engine_component_iterator_t end{};
    engineComponentViewCreateBeginComponentIterator(cv, &begin);
    engineComponentViewCreateEndComponentIterator(cv, &end);

    std::vector<engine_game_object_t> ret{};
    while (!engineComponentIteratorCheckEqual(begin, end))
    {
        auto go_it = engineComponentIteratorGetGameObject(begin);
        if (engineSceneHasNameComponent(scene, go_it))
        {
            if (0 == std::strcmp(engineGetNameComponent(scene, go_it).name, name.data()))
            {
                ret.push_back(go_it);
            }
        }
        engineComponentIteratorNext(begin);
    }
    engineDestroyComponentView(cv);
    return ret;
}

glm::quat project_c::utils::rotate_toward(glm::vec3 origin, glm::vec3 target)
{
    const auto dir = glm::normalize(target - origin);
    const auto angle = glm::degrees(std::atan2(dir.x, dir.z));
    return glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
}

engine_ray_t project_c::utils::get_ray_from_mouse_position(engine_application_t app, engine_scene_t scene, engine_game_object_t go_camera)
{
    engine_ray_t ray{};
    // ray origin
    const auto camera_transform = engineSceneGetTransformComponent(scene, go_camera);
    ray.origin.x = camera_transform.position[0];
    ray.origin.y = camera_transform.position[1];
    ray.origin.z = camera_transform.position[2];

    const auto mouse_coords = engineApplicationGetMouseCoords(app);
    ray.direction = engineSceneCameraComponentConvertSpacePositionToWorldPosition(scene, go_camera, engine_fvec3_t{mouse_coords.x, mouse_coords.y, 1.0f});
    return ray;
}

