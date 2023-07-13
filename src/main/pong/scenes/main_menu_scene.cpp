#include "main_menu_scene.h"
#include "../scripts/camera_script.h"
#include "scene_manager.h"

#include "battleground_scene.h"

#include <iostream>

void start_pve_scene(const engine_ui_event_t* event, void* user_data_ptr)
{
    assert(user_data_ptr);

    auto main_menu_scene = reinterpret_cast<pong::MainMenuScene*>(user_data_ptr);
    main_menu_scene->deactivate();
    main_menu_scene->get_scene_manager()->register_scene<pong::BattlegroundScene>(pong::BattlegroundScene::PlayerType::eBotLow);
    main_menu_scene->get_scene_manager()->get_scene(pong::BattlegroundScene::get_name())->activate();
}

void start_pvp_scene(const engine_ui_event_t* event, void* user_data_ptr)
{
    assert(user_data_ptr);

    auto main_menu_scene = reinterpret_cast<pong::MainMenuScene*>(user_data_ptr);
    main_menu_scene->deactivate();
    main_menu_scene->get_scene_manager()->register_scene<pong::BattlegroundScene>(pong::BattlegroundScene::PlayerType::eHuman);
    main_menu_scene->get_scene_manager()->get_scene(pong::BattlegroundScene::get_name())->activate();
}

pong::MainMenuScene::MainMenuScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
    : IScene(app_handle, scn_mgn, engine_error_code)
{
    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        // load ui doc
        engine_error_code = engineApplicationCreateUiDocumentFromFile(app_handle, "pong_main_menu.rml", &ui_doc_);
        if (ui_doc_)
        {
            engineUiDocumentShow(ui_doc_);
        }

        engine_error_code = engineUiDocumentGetElementById(ui_doc_, "id_start_pve_scene", &ui_element_start_pve_scene_);
        engineUiElementAddEventCallback(ui_element_start_pve_scene_, ENGINE_UI_EVENT_TYPE_POINTER_CLICK, this, start_pve_scene);

        engine_error_code = engineUiDocumentGetElementById(ui_doc_, "id_start_pvp_scene", &ui_element_start_pvp_scene_);
        engineUiElementAddEventCallback(ui_element_start_pvp_scene_, ENGINE_UI_EVENT_TYPE_POINTER_CLICK, this, start_pvp_scene);
    }
}

pong::MainMenuScene::~MainMenuScene()
{
}

void pong::MainMenuScene::deactivate()
{
    engineUiDocumentHide(ui_doc_);
    IScene::deactivate();
}
