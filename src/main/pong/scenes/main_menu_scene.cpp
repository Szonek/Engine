#include "main_menu_scene.h"
#include "../scripts/camera_script.h"
#include "../scripts/ui_scripts.h"

pong::MainMenuScene::MainMenuScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
    : IScene(app_handle, scn_mgn, engine_error_code)
{
    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        // first create data model handel
        std::array<engine_ui_document_data_binding_t, 2> bindings;

        //std::strcpy(data_model.name, "animals");

        std::strcpy(bindings[0].name, "show_text");
        bindings[0].type = ENGINE_DATA_TYPE_BOOL;
        bindings[0].data_bool = &my_data_.show_text;

        std::strcpy(bindings[1].name, "score");
        bindings[1].type = ENGINE_DATA_TYPE_UINT32;
        bindings[1].data_uint32_t = &my_data_.score;

        // = engineApplicationUI
        engine_error_code = engineApplicationCreateUiDocumentDataHandle(app_handle, "animals", bindings.data(), bindings.size(), &ui_data_handle_);
        if (engine_error_code != ENGINE_RESULT_CODE_OK) 
        {
            return;
        }
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