#include "main_menu_scene.h"
#include "../scripts/camera_script.h"
#include "../scripts/ui_scripts.h"

pong::MainMenuScene::MainMenuScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
    : IScene(app_handle, scn_mgn, engine_error_code)
{
    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        // load ui doc
        engine_error_code = engineApplicationCreateUiDocumentFromFile(app_handle, "pong_main_menu.rml", &ui_doc_);
        if (ui_doc_)
        {
            engineApplicationUiDocumentShow(app_handle, ui_doc_);
        }
    }

    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        // register scripts
        auto camera_script = register_script<CameraScript>();
        auto start_pve = register_script<MainMenuStartPveScene>();
        auto start_pvp = register_script<MainMenuStartPvpScene>();
    }
}