#include "pve_scene.h"

#include "../scripts/global_constants.h"
#include "../scripts/ball_script.h"
#include "../scripts/player_paddle_script.h"
#include "../scripts/goal_net_script.h"
#include "../scripts/wall_script.h"
#include "../scripts/camera_script.h"
#include "../scripts/ui_scripts.h"

#include "../scripts/global_constants.h"

void right_controller_callback(const engine_ui_event_t* event, void* user_data_ptr)
{
    using namespace pong;
    assert(user_data_ptr);
    auto paddle_script = reinterpret_cast<pong::RightPlayerPaddleScript*>(user_data_ptr);

    const float full_screen_height = K_CAMERA_ORTHO_SCALE * 2.0f;
    const float top_wall_screenspace_pos = (K_CAMERA_ORTHO_SCALE + K_WALL_Y_OFFSET) / full_screen_height;
    const float bottom_wall_screenspace_pos = (K_CAMERA_ORTHO_SCALE - K_WALL_Y_OFFSET) / full_screen_height;
    const float screenspace_table_height = top_wall_screenspace_pos - bottom_wall_screenspace_pos;
    const float pointer_pos = event->normalized_screen_position.y;
    const float pointer_screenspace_scaled_to_table = (pointer_pos * screenspace_table_height) + bottom_wall_screenspace_pos;
    // map to range <-1, 1> and scale to world space size  (world middle is 0,0, so we need negative values to go down/left)!
    const float pointer_world_space = K_CAMERA_ORTHO_SCALE * ((pointer_screenspace_scaled_to_table * 2.0f) - 1.0f);
    paddle_script->set_target_worldspace_position(pointer_world_space);

}


pong::PveScene::PveScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
    : IScene(app_handle, scn_mgn, engine_error_code)
{

    // create ui first
    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
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

        // right ui
        //auto right_touch_area_script = register_script<RightPlayerTouchAreaScript>();
        //auto right_superpower_0_script = register_script<RightPlayerSuperPower_0_TouchAreaScript>();
        //right_superpower_0_script->player_script_ = right_player_script;

        //right_touch_area_script->player_script_ = right_player_script;

        right_player_script->ball_script_ = ball_script;
        left_player_script->ball_script_ = ball_script;

        left_goal_net_script->ball_script_ = ball_script;
        left_goal_net_script->player_paddel_script_ = right_player_script;
        right_goal_net_script->ball_script_ = ball_script;
        right_goal_net_script->player_paddel_script_ = left_player_script;


        engineUiElementAddEventCallback(ui_element_right_controller_, ENGINE_UI_EVENT_TYPE_CLICK, right_player_script, right_controller_callback);
    }


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
