#pragma once
#include "iscene.h"

namespace pong
{
class MainMenuScene : public engine::IScene
{
public:
    MainMenuScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code);
    ~MainMenuScene();
    static constexpr const char* get_name() { return "main_menu_scene"; }

    void deactivate() override;

private:
    engine_ui_document_t ui_doc_ = nullptr;
    engine_ui_element_t ui_element_start_pve_scene_ = nullptr;
    engine_ui_element_t ui_element_start_pvp_scene_ = nullptr;
};
}