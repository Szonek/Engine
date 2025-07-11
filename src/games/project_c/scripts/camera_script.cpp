#include "camera_script.h"
#include "scripts_utils.h"

#include "iscene.h"

project_c::CameraScript::CameraScript(engine::IScene* my_scene)
    : BaseNode(my_scene, "camera")
{
    auto camera_comp = engineAddCameraComponent(go_);
    camera_comp.enabled = true;
    camera_comp.clip_plane_near = 0.1f;
    camera_comp.clip_plane_far = 1000.0f;
    camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
    camera_comp.type_union.perspective_fov = 45.0f;

    camera_comp.target[0] = 0.0f;
    camera_comp.target[1] = 0.0f;
    camera_comp.target[2] = 0.0f;

    engineUpdateCameraComponent(go_, &camera_comp);

    auto camera_transform_comp = engineAddTransformComponent(go_);
    camera_transform_comp.position[0] = 0.0f;
    camera_transform_comp.position[1] = 4.0f;
    camera_transform_comp.position[2] = 1.0f;
    engineUpdateTransformComponent(go_, &camera_transform_comp);
}

void project_c::CameraScript::late_update(float dt)
{
    engine::ScopedProfiler profiler("project_c::CameraScript::late_update");
    const auto mouse_coords = engineGetMouseCoords();

    const float move_speed = 1.0f * dt;
    const auto all_players = utils::get_game_objects_with_name("player");
    const auto character_go = all_players.size() > 0 ? all_players[0] : ENGINE_INVALID_GAME_OBJECT_ID;
    // follow character go
    if (character_go != ENGINE_INVALID_GAME_OBJECT_ID)
    {
        //first update position
        const auto character_tc = engineGetTransformComponent(character_go);
        auto tc = engineGetTransformComponent(go_);
        tc.position[0] = character_tc.position[0];
        tc.position[1] = character_tc.position[1] + 6.5f;
        tc.position[2] = character_tc.position[2] + 4.5f;
        engineUpdateTransformComponent(go_, &tc);
        // update targer to point to character go position
        auto camera_comp = engineGetCameraComponent(go_);
        camera_comp.target[0] = character_tc.position[0];
        camera_comp.target[1] = character_tc.position[1];
        camera_comp.target[2] = character_tc.position[2];
        engineUpdateCameraComponent(go_, &camera_comp);
    }
}