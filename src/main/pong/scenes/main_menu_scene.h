#pragma once
#include "iscene.h"

namespace pong
{
class MainMenuScene : public engine::IScene
{
public:
    static constexpr const char* K_NAME = "main_menu_scene";
public:
    MainMenuScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code);
};
}