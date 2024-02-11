#include "battleground_ui_script.h"
#include "player_paddle_script.h"
#include "global_constants.h"
#include "iscene.h"

#include <cassert>
#include <array>




void right_controller_callback_start_move(const engine_ui_event_t* event, void* user_data_ptr)
{
    using namespace pong;
    assert(user_data_ptr);
    auto scene_data = reinterpret_cast<pong::BattlegroundUiScript::MyDataForUI*>(user_data_ptr);
    scene_data->player_right.move_allowed = true;
}


void right_controller_callback_stop_move(const engine_ui_event_t* event, void* user_data_ptr)
{
    using namespace pong;
    assert(user_data_ptr);
    auto scene_data = reinterpret_cast<pong::BattlegroundUiScript::MyDataForUI*>(user_data_ptr);
    scene_data->player_right.move_allowed = false;
}

void right_controller_callback_move(const engine_ui_event_t* event, void* user_data_ptr)
{
    using namespace pong;
    assert(user_data_ptr);
    auto scene_data = reinterpret_cast<pong::BattlegroundUiScript::MyDataForUI*>(user_data_ptr);

    if (!scene_data->player_right.move_allowed)
    {
        return;
    }

    const float full_screen_height = K_CAMERA_ORTHO_SCALE * 2.0f;
    const float top_wall_screenspace_pos = (K_CAMERA_ORTHO_SCALE + K_WALL_Y_OFFSET) / full_screen_height;
    const float bottom_wall_screenspace_pos = (K_CAMERA_ORTHO_SCALE - K_WALL_Y_OFFSET) / full_screen_height;
    const float screenspace_table_height = top_wall_screenspace_pos - bottom_wall_screenspace_pos;
    const float pointer_pos = event->normalized_screen_position.y;
    const float pointer_screenspace_scaled_to_table = (pointer_pos * screenspace_table_height) + bottom_wall_screenspace_pos;
    // map to range <-1, 1> and scale to world space size  (world middle is 0,0, so we need negative values to go down/left)!
    const float pointer_world_space = K_CAMERA_ORTHO_SCALE * ((pointer_screenspace_scaled_to_table * 2.0f) - 1.0f);
    scene_data->player_right.script->set_target_worldspace_position(pointer_world_space);

}

void trigger_superpower(const engine_ui_event_t* event, void* user_data_ptr)
{
    using namespace pong;
    assert(user_data_ptr);
    auto scene_data = reinterpret_cast<pong::BattlegroundUiScript::MyDataForUI*>(user_data_ptr);
    scene_data->player_right.script->trigger_super_power();
}

pong::BattlegroundUiScript::BattlegroundUiScript(engine::IScene* my_scene)
    : IScript(my_scene)
{
    engine_result_code_t engine_error_code = ENGINE_RESULT_CODE_OK;
    // create ui first
    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        // first create data model handel
        std::array<engine_ui_document_data_binding_t, 4> bindings;

        //std::strcpy(bindings[0].name, "score_left");
        bindings[0].type = ENGINE_DATA_TYPE_UINT32;
        bindings[0].data_uint32_t = &my_data_.player_left.score;

        //std::strcpy(bindings[1].name, "score_right");
        bindings[1].type = ENGINE_DATA_TYPE_UINT32;
        bindings[1].data_uint32_t = &my_data_.player_right.score;

        //std::strcpy(bindings[2].name, "show_left_player_move_controller");
        bindings[2].type = ENGINE_DATA_TYPE_BOOL;
        bindings[2].data_bool = &my_data_.player_left.show_controllos;

        //std::strcpy(bindings[3].name, "show_right_player_move_controller");
        bindings[3].type = ENGINE_DATA_TYPE_BOOL;
        bindings[3].data_bool = &my_data_.player_right.show_controllos;

        engine_error_code = engineApplicationCreateUiDocumentDataHandle(my_scene_->get_app_handle(), "scene_ui_data", bindings.data(), bindings.size(), &ui_data_handle_);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            return;
        }

        engine_error_code = engineApplicationCreateUiDocumentFromFile(my_scene_->get_app_handle(), "pong_battleground.rml", &ui_doc_);
        if (ui_doc_)
        {
            engineUiDocumentShow(ui_doc_);
        }
    }

    {
        engine_ui_element_t ui_ele = nullptr;
        engine_error_code = engineUiDocumentGetElementById(ui_doc_, "right_controller", &ui_ele);

        engineUiElementAddEventCallback(ui_ele, ENGINE_UI_EVENT_TYPE_POINTER_UP, &my_data_, right_controller_callback_stop_move);
        engineUiElementAddEventCallback(ui_ele, ENGINE_UI_EVENT_TYPE_POINTER_DOWN, &my_data_, right_controller_callback_start_move);
        engineUiElementAddEventCallback(ui_ele, ENGINE_UI_EVENT_TYPE_POINTER_MOVE, &my_data_, right_controller_callback_move);
    }

    {
        engine_ui_element_t ui_ele = nullptr;
        engine_error_code = engineUiDocumentGetElementById(ui_doc_, "right_controller_superpower", &ui_ele);

        engineUiElementAddEventCallback(ui_ele, ENGINE_UI_EVENT_TYPE_POINTER_CLICK, &my_data_, trigger_superpower);
    }

}

void pong::BattlegroundUiScript::set_left_player_script(PlayerPaddleScript* script, bool is_bot)
{
    my_data_.player_left.script = script;
    my_data_.player_left.show_controllos = !is_bot;
    engineUiDataHandleDirtyVariable(ui_data_handle_, "show_left_player_move_controller");
}

void pong::BattlegroundUiScript::set_right_player_script(PlayerPaddleScript* script)
{
    my_data_.player_right.script = script;
    my_data_.player_right.show_controllos = true;
    engineUiDataHandleDirtyVariable(ui_data_handle_, "show_right_player_move_controller");
}

void pong::BattlegroundUiScript::update(float dt)
{
    if (my_data_.player_right.script->get_score() != my_data_.player_right.score)
    {
        my_data_.player_right.score = my_data_.player_right.script->get_score();
        engineUiDataHandleDirtyVariable(ui_data_handle_, "score_right");
    }
    if (my_data_.player_left.script->get_score() != my_data_.player_left.score)
    {
        my_data_.player_left.score = my_data_.player_left.script->get_score();
        engineUiDataHandleDirtyVariable(ui_data_handle_, "score_left");
    }

    //if (my_data_.player_right.script->get_super_power_state() == SuperPowerState::eNone)
    if(false)
    {
        engine_ui_element_t ui_ele{};
        engineUiDocumentGetElementById(ui_doc_, "right_controller_superpower", &ui_ele);
        engineUiElementRemoveProperty(ui_ele, "image-color");
    }
    //else if (my_data_.player_right.script->get_super_power_state() == SuperPowerState::eOnCooldown)
    else if(false)
    {
        engine_ui_element_t ui_ele{};
        engineUiDocumentGetElementById(ui_doc_, "right_controller_superpower", &ui_ele);
        engineUiElementSetProperty(ui_ele, "image-color", "rgb(128, 128, 128)");
    }
}
