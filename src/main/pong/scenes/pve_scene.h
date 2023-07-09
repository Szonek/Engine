#pragma once
#include "iscene.h"

namespace pong
{
class PveScene : public engine::IScene
{
public:
    PveScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code);

public:
    static constexpr const char* get_name() { return "pve_scene"; }

    void activate() override;
    void deactivate() override;

private:
    engine_ui_data_handle_t ui_data_handle_ = nullptr;
    engine_ui_document_t ui_doc_ = nullptr;
    engine_ui_element_t ui_element_left_controller = nullptr;
    engine_ui_element_t ui_element_right_controller_ = nullptr;
};
}