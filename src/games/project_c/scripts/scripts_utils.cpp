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
            if (0 == std::strcmp(engineSceneGetNameComponent(scene, go_it).name, name.data()))
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
    ray.origin[0] = camera_transform.position[0];
    ray.origin[1] = camera_transform.position[1];
    ray.origin[2] = camera_transform.position[2];

    //unproject mouse coords to 3d space to get direction
    glm::vec4 viewport(0.0f, 0.0f, 1140.0f, 540.0f);
    const auto camera = engineSceneGetCameraComponent(scene, go_camera);
    const auto mouse_coords = engineApplicationGetMouseCoords(app);
    const auto mouse_x = mouse_coords.x * viewport.z;
    const auto mouse_y = mouse_coords.y * viewport.w;

    glm::mat4 view = glm::mat4(0.0);
    glm::mat4 projection = glm::mat4(0.0);
    // update camera: view and projection
    {
        const auto z_near = camera.clip_plane_near;
        const auto z_far = camera.clip_plane_far;
        // ToD: multi camera - this should use resolution of camera!!!

        const auto adjusted_width = viewport.z * (camera.viewport_rect.width - camera.viewport_rect.x);
        const auto adjusted_height = viewport.w * (camera.viewport_rect.height - camera.viewport_rect.y);
        const float aspect = adjusted_width / adjusted_height;

        if (camera.type == ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC)
        {
            const float scale = camera.type_union.orthographics_scale;
            projection = glm::ortho(-aspect * scale, aspect * scale, -scale, scale, z_near, z_far);
        }
        else
        {
            projection = glm::perspective(glm::radians(camera.type_union.perspective_fov), aspect, z_near, z_far);
        }
        const auto eye_position = glm::make_vec3(camera_transform.position);
        const auto up = glm::make_vec3(camera.direction.up);
        const auto target = glm::make_vec3(camera.target);
        view = glm::lookAt(eye_position, target, up);
    }

    const auto ray_dir = glm::unProject(glm::vec3(mouse_x, mouse_y, 1.0f), view, projection, viewport);
    ray.direction[0] = ray_dir.x;
    ray.direction[1] = ray_dir.y;
    ray.direction[2] = ray_dir.z;
    return ray;
}

