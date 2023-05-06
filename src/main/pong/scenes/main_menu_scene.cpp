#pragma once
#include "main_menu_scene.h"
#include "../scripts/camera_script.h"
#include "../scripts/wall_script.h"



class RandomWallScript : public WallScript
{
public:
    RandomWallScript(engine_application_t& app, engine_scene_t& scene)
        : WallScript(app, scene, 0.0f, "random")
    {
    }
};

pong::MainMenuScene::MainMenuScene(engine_application_t app_handle, engine_result_code_t& engine_error_code)
    : IScene(app_handle, engine_error_code)
{
    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        auto camera_script = register_script<CameraScript>();
        auto random_wall_script = register_script<RandomWallScript>();

    }
}