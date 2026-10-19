#include <profiler.h>
#include "scripts_utils.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>

std::vector<engine_game_object_t> project_c::utils::get_children(engine_game_object_t go)
{
    std::vector<engine_game_object_t> ret(engineGameObjectGetChildrenCount(go));
    for (std::size_t i = 0; i < ret.size(); i++)
    {
        ret[i] = engineGameObjectGetChild(go, i);
    }
    return ret;
}

void project_c::utils::delete_game_objects_hierarchy(engine_game_object_t go)
{
    // a copy: destroying a child changes the list
    for (const auto child : get_children(go))
    {
        delete_game_objects_hierarchy(child);
        engineGameObjectDestroy(child);
    }
}

std::vector<engine_game_object_t> project_c::utils::get_active_camera_game_objects()
{
    engine::ScopedProfiler prof("project_c::utils::get_active_camera_game_objects");
    engine_component_view_t cv{};
    engineComponentViewCreate(&cv);
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
    engineComponentViewDestroy(cv);
    return ret;
}

std::vector<engine_game_object_t> project_c::utils::get_game_objects_with_name(std::string_view name)
{
    engine::ScopedProfiler prof("project_c::utils::get_game_objects_with_name");
    engine_component_view_t cv{};
    engineComponentViewCreate(&cv);
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
    engineComponentViewDestroy(cv);
    return ret;
}

glm::quat project_c::utils::rotate_toward(glm::vec3 origin, glm::vec3 target)
{
    const auto dir = glm::normalize(target - origin);
    const auto angle = glm::degrees(std::atan2(dir.x, dir.z));
    return glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
}

engine_ray_t project_c::utils::get_ray_from_cursor(engine_game_object_t go_camera, engine_fvec2_t cursor)
{
    engine_ray_t ray{};
    // from the point under the cursor on the near plane to the one on the far plane.
    // Works for both projections (orthographic rays are parallel, so they can't start at the camera position).
    ray.origin = engineCameraComponentConvertSpacePositionToWorldPosition(go_camera, engine_fvec3_t{cursor.x, cursor.y, 0.0f});
    ray.direction = engineCameraComponentConvertSpacePositionToWorldPosition(go_camera, engine_fvec3_t{cursor.x, cursor.y, 1.0f});
    return ray;
}

