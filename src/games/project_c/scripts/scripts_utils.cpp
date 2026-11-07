#include <profiler.h>
#include "scripts_utils.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstring>

std::vector<engine_game_object_t> project_c::utils::get_children(engine_game_object_t go)
{
    std::vector<engine_game_object_t> ret(engineGameObjectGetChildrenCount(go));
    for (std::size_t i = 0; i < ret.size(); i++)
    {
        ret[i] = engineGameObjectGetChild(go, i);
    }
    return ret;
}

void project_c::utils::set_name(engine_game_object_t go, std::string_view name)
{
    auto nc = engineGameObjectHasNameComponent(go) ? engineGameObjectGetNameComponent(go) : engineGameObjectAddNameComponent(go);
    std::memset(nc.name, 0, sizeof(nc.name));
    std::memcpy(nc.name, name.data(), std::min(name.size(), sizeof(nc.name) - 1));
    engineGameObjectUpdateNameComponent(go, &nc);
}

glm::vec3 project_c::utils::get_world_position(engine_game_object_t go)
{
    if (!engineGameObjectHasTransformComponent(go))
    {
        return glm::vec3(0.0f);
    }
    auto tc = engineGameObjectGetTransformComponent(go);
    glm::vec3 position = glm::make_vec3(tc.position);
    // through the parents' transforms: the world transform component is only computed by the scene update
    for (auto parent = go; engineGameObjectHasParentComponent(parent);)
    {
        parent = engineGameObjectGetParentComponent(parent).parent;
        if (!engineGameObjectHasTransformComponent(parent))
        {
            break;
        }
        tc = engineGameObjectGetTransformComponent(parent);
        position = glm::make_vec3(tc.position) + glm::make_quat(tc.rotation) * (glm::make_vec3(tc.scale) * position);
    }
    return position;
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

glm::quat project_c::utils::rotate_toward(glm::vec3 origin, glm::vec3 target)
{
    const auto dir = glm::normalize(target - origin);
    const auto angle = glm::degrees(std::atan2(dir.x, dir.z));
    return glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
}

project_c::utils::CursorRay project_c::utils::get_ray_from_cursor(engine_game_object_t go_camera, engine_fvec2_t cursor)
{
    // from the point under the cursor on the near plane to the one on the far plane.
    // Works for both projections (orthographic rays are parallel, so they can't start at the camera position).
    const auto near_point = engineCameraComponentConvertSpacePositionToWorldPosition(go_camera, engine_fvec3_t{cursor.x, cursor.y, 0.0f});
    const auto far_point = engineCameraComponentConvertSpacePositionToWorldPosition(go_camera, engine_fvec3_t{cursor.x, cursor.y, 1.0f});
    CursorRay ray{};
    ray.origin = glm::vec3(near_point.x, near_point.y, near_point.z);
    ray.direction = glm::vec3(far_point.x, far_point.y, far_point.z) - ray.origin;
    return ray;
}

bool project_c::utils::level_geometry_between(const glm::vec3& from, const glm::vec3& to, std::span<const engine_game_object_t> ignore)
{
    constexpr std::size_t max_see_through = 4;
    std::vector<engine_game_object_t> skipped(ignore.begin(), ignore.end());
    const glm::vec3 direction = to - from;
    if (glm::length(direction) <= 0.0f)
    {
        return false;
    }
    for (std::size_t cast = 0; cast <= max_see_through; cast++)
    {
        engine_physics_query_filter_t filter{};
        filter.ignore = skipped.data();
        filter.ignore_count = skipped.size();
        engine_ray_hit_t hit{};
        if (!enginePhysicsRayCast(glm::value_ptr(from), glm::value_ptr(direction), glm::length(direction), &filter, &hit))
        {
            return false;
        }
        bool dynamic = false;
        if (engineGameObjectHasRigidBodyComponent(hit.go))
        {
            const auto body = engineGameObjectGetRigidBodyComponent(hit.go);
            dynamic = body.type == ENGINE_RIGID_BODY_TYPE_DYNAMIC && body.mass > 0.0f;
        }
        if (!dynamic && !engineGameObjectHasCharacterControllerComponent(hit.go))
        {
            return true;
        }
        skipped.push_back(hit.go);
    }
    return true;
}
