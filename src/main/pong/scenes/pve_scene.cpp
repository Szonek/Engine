#include "pve_scene.h"
#include "../scripts/event_types.h"

#include "../scripts/global_constants.h"
#include "../scripts/ball_script.h"
#include "../scripts/player_paddle_script.h"
#include "../scripts/goal_net_script.h"
#include "../scripts/wall_script.h"
#include "../scripts/camera_script.h"

#include "../scripts/global_constants.h"



void right_controller_callback_start_move(const engine_ui_event_t* event, void* user_data_ptr)
{
    using namespace pong;
    assert(user_data_ptr);
    auto scene_data = reinterpret_cast<pong::PveScene::MyDataForUI*>(user_data_ptr);
    scene_data->move_right_player = true;
}


void right_controller_callback_stop_move(const engine_ui_event_t* event, void* user_data_ptr)
{
    using namespace pong;
    assert(user_data_ptr);
    auto scene_data = reinterpret_cast<pong::PveScene::MyDataForUI*>(user_data_ptr);
    scene_data->move_right_player = false;
}

void right_controller_callback_move(const engine_ui_event_t* event, void* user_data_ptr)
{
    using namespace pong;
    assert(user_data_ptr);
    auto scene_data = reinterpret_cast<pong::PveScene::MyDataForUI*>(user_data_ptr);

    if (!scene_data->move_right_player)
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
    scene_data->right_player->set_target_worldspace_position(pointer_world_space);

}


pong::PveScene::PveScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
    : IScene(app_handle, scn_mgn, engine_error_code)
{
    my_data_.show_right_player_controllors = true;
    // create ui first
    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        // first create data model handel
        std::array<engine_ui_document_data_binding_t, 4> bindings;

        std::strcpy(bindings[0].name, "score_left");
        bindings[0].type = ENGINE_DATA_TYPE_UINT32;
        bindings[0].data_uint32_t = &my_data_.score_left;

        std::strcpy(bindings[1].name, "score_right");
        bindings[1].type = ENGINE_DATA_TYPE_UINT32;
        bindings[1].data_uint32_t = &my_data_.score_right;

        std::strcpy(bindings[2].name, "show_left_player_move_controller");
        bindings[2].type = ENGINE_DATA_TYPE_BOOL;
        bindings[2].data_bool = &my_data_.show_left_player_controllors;

        std::strcpy(bindings[3].name, "show_right_player_move_controller");
        bindings[3].type = ENGINE_DATA_TYPE_BOOL;
        bindings[3].data_bool = &my_data_.show_right_player_controllors;

        engine_error_code = engineApplicationCreateUiDocumentDataHandle(app_handle, "scene_ui_data", bindings.data(), bindings.size(), &ui_data_handle_);
        if (engine_error_code != ENGINE_RESULT_CODE_OK) 
        {
            return;
        }

        engine_error_code = engineApplicationCreateUiDocumentFromFile(app_handle, "pong_battleground.rml", &ui_doc_);
        if (ui_doc_)
        {
            engineUiDocumentShow(ui_doc_);
        }

        engine_error_code = engineUiDocumentGetElementById(ui_doc_, "right_controller", &ui_element_right_controller_);
    }

    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        const float gravity[3] = { 0.0f, 0.0f, 0.0f };
        engineSceneSetGravityVector(scene_, gravity);

        // core
        auto camera_script = register_script<CameraScript>();
        auto ball_script = register_script<BallScript>();
        // players
        auto left_player_script = register_script<BotPlayerPaddleScript>();
        auto right_player_script = register_script<RightPlayerPaddleScript>();
        // goal nets
        auto left_goal_net_script = register_script<LeftGoalNetScript>();
        auto right_goal_net_script = register_script<RightGoalNetScript>();
        // walls
        auto top_wall = register_script<WallTopScript>();
        auto bottom_wall = register_script<BottomTopScript>();

        right_player_script->ball_script_ = ball_script;
        left_player_script->ball_script_ = ball_script;

        left_goal_net_script->ball_script_ = ball_script;
        left_goal_net_script->player_paddel_script_ = right_player_script;
        right_goal_net_script->ball_script_ = ball_script;
        right_goal_net_script->player_paddel_script_ = left_player_script;

        my_data_.right_player = right_player_script;
        my_data_.left_player = left_player_script;
        engineUiElementAddEventCallback(ui_element_right_controller_, ENGINE_UI_EVENT_TYPE_POINTER_UP, &my_data_, right_controller_callback_stop_move);
        engineUiElementAddEventCallback(ui_element_right_controller_, ENGINE_UI_EVENT_TYPE_POINTER_DOWN, &my_data_, right_controller_callback_start_move);
        engineUiElementAddEventCallback(ui_element_right_controller_, ENGINE_UI_EVENT_TYPE_POINTER_MOVE, &my_data_, right_controller_callback_move);
    }

    register_event_callback(PONG_EVENT_TYPE_GOAL_SCORED, [this]()
        {
            my_data_.score_right = my_data_.right_player->get_score();
            my_data_.score_left = my_data_.left_player->get_score();
            engineUiDataHandleDirtyAllVariables(ui_data_handle_);
        }
    );

    // deactivate at start
    deactivate();
}

void pong::PveScene::activate()
{
    engineUiDocumentShow(ui_doc_);
    IScene::activate();
}


void pong::PveScene::deactivate()
{
    engineUiDocumentHide(ui_doc_);
    IScene::deactivate();
}
