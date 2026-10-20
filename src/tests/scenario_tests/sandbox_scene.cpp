#include "sandbox_scene.h"

#include <app.h>
#include <scripts/camera_script.h>
#include <scripts/encounter_script.h>
#include <scripts/enviorment_script.h>
#include <scripts/player_script.h>
#include <scripts/scripts_utils.h>

scenario::SandboxScene::SandboxScene(engine::IApplication* app)
    : IScene(app)
    , builder_(*static_cast<project_c::AppProjectC*>(app), *this)
{
    builder_.nav_surface();
    camera_ = register_script<project_c::CameraScript>();
    register_script<project_c::MainLight>();
    encounter_ = register_script<project_c::Encounter>();
}

engine_result_code_t scenario::SandboxScene::update(float dt)
{
    if (!nav_mesh_built_)
    {
        nav_mesh_built_ = true;
        // a scene without a floor has no nav mesh; its enemies don't move
        engineNavMeshBuild();
    }
    return IScene::update(dt);
}

void scenario::SandboxScene::snap_camera_to_player()
{
    camera_->snap_to_player();
}

project_c::Player* scenario::SandboxScene::player()
{
    return find_script<project_c::Player>();
}

void scenario::GameplayTest::step_aiming(std::uint32_t frames, project_c::PlayerInput input, const glm::vec3& target)
{
    for (std::uint32_t i = 0; i < frames; i++)
    {
        const auto cameras = project_c::utils::get_active_camera_game_objects();
        if (!cameras.empty())
        {
            const float world[3] = { target.x, target.y, target.z };
            const auto screen = engineCameraComponentConvertWorldPositionToScreenPosition(cameras.front(), world);
            input.cursor = { screen.x, screen.y };
        }
        set_input(input);
        step();
    }
}

glm::vec3 scenario::GameplayTest::position_of(engine_game_object_t go)
{
    const auto tc = engineGameObjectGetTransformComponent(go);
    return { tc.position[0], tc.position[1], tc.position[2] };
}
