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

private:
    struct MainMenuData 
    {
        bool show_text = true;
        std::uint32_t score = 0;
    };

private:
    MainMenuData my_data_ = {};
    engine_ui_document_data_handle_t ui_data_handle_;
    engine_ui_document_t ui_doc_ = nullptr;
};
}