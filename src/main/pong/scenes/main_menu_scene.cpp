#include "main_menu_scene.h"
#include "../scripts/camera_script.h"
#include "../scripts/ui_scripts.h"


pong::MainMenuScene::MainMenuScene(engine_application_t app_handle, engine_result_code_t& engine_error_code, engine::IScene* temp)
    : IScene(app_handle, engine_error_code)
{
    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        auto camera_script = register_script<CameraScript>();
        auto start_pve = register_script<MainMenuStartPveScene>();
        start_pve->my_scene_ = this;
        start_pve->pve_scene_ = temp;
    }
}