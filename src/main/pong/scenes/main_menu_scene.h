#pragma once
#include "iscene.h"

namespace pong
{
class MainMenuScene : public engine::IScene
{
public:
    MainMenuScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code);

public:
    static constexpr const char* get_name() { return "main_menu_scene"; }

private:
    engine_ui_document_t ui_doc_ = nullptr;
};
}