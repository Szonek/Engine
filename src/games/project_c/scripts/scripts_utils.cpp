#include <profiler.h>
#include "scripts_utils.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>

void project_c::utils::delete_game_objects_hierarchy(engine_game_object_t go)
{
    if (engineGameObjectHasChildrenComponent(go))
    {
        auto cc = engineGameObjectGetChildrenComponent(go);
        for (auto i = 0; i < std::size(cc.child); i++)
        {
            if (cc.child[i] != ENGINE_INVALID_GAME_OBJECT_ID)
            {
                delete_game_objects_hierarchy(cc.child[i]);
                engineDestroyGameObject(cc.child[i]);
            }
        }
    }
}

std::vector<engine_game_object_t> project_c::utils::get_active_camera_game_objects()
{
    engine::ScopedProfiler prof("project_c::utils::get_active_camera_game_objects");
    engine_component_view_t cv{};
    engineCreateComponentView(&cv);
    engineComponentViewAttachCameraComponent(cv);

    engine_component_iterator_t begin{};
    engine_component_iterator_t end{};
    engineComponentViewCreateBeginComponentIterator(cv, &begin);
    engineComponentViewCreateEndComponentIterator(cv, &end);

    std::vector<engine_game_object_t> ret{};
    while (!engineComponentIteratorCheckEqual(begin, end))
    {
        auto go_it = engineComponentIteratorGetGameObject(begin);
        if (engineGameObjectHasCameraComponent(go_it))
        {
            if (engineGameObjectGetCameraComponent(go_it).enabled)
            {
                ret.push_back(go_it);
            }
        }
        engineComponentIteratorNext(begin);
    }
    engineDestroyComponentView(cv);
    return ret;
}

std::vector<engine_game_object_t> project_c::utils::get_game_objects_with_name(std::string_view name)
{
    engine::ScopedProfiler prof("project_c::utils::get_game_objects_with_name");
    engine_component_view_t cv{};
    engineCreateComponentView(&cv);
    engineComponentViewAttachNameComponent(cv);

    engine_component_iterator_t begin{};
    engine_component_iterator_t end{};
    engineComponentViewCreateBeginComponentIterator(cv, &begin);
    engineComponentViewCreateEndComponentIterator(cv, &end);

    std::vector<engine_game_object_t> ret{};
    while (!engineComponentIteratorCheckEqual(begin, end))
    {
        auto go_it = engineComponentIteratorGetGameObject(begin);
        if (engineGameObjectHasNameComponent(go_it))
        {
            if (0 == std::strcmp(engineGameObjectGetNameComponent(go_it).name, name.data()))
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

engine_ray_t project_c::utils::get_ray_from_mouse_position(engine_game_object_t go_camera)
{
    engine_ray_t ray{};
    // ray origin
    const auto camera_transform = engineGameObjectGetTransformComponent(go_camera);
    ray.origin.x = camera_transform.position[0];
    ray.origin.y = camera_transform.position[1];
    ray.origin.z = camera_transform.position[2];

    const auto mouse_coords = engineGetMouseCoords();
    ray.direction = engineCameraComponentConvertSpacePositionToWorldPosition(go_camera, engine_fvec3_t{mouse_coords.x, mouse_coords.y, 1.0f});
    return ray;
}

