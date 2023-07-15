#include "battleground_scene.h"
#include "../scripts/event_types.h"

#include "../scripts/global_constants.h"
#include "../scripts/ball_script.h"
#include "../scripts/player_paddle_script.h"
#include "../scripts/goal_net_script.h"
#include "../scripts/wall_script.h"
#include "../scripts/camera_script.h"
#include "../scripts/battleground_ui_script.h"

#include "../scripts/global_constants.h"


pong::BattlegroundScene::BattlegroundScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code, PlayerType left_player_type)
    : IScene(app_handle, scn_mgn, engine_error_code)
{
    if (engine_error_code == ENGINE_RESULT_CODE_OK)
    {
        const float gravity[3] = { 0.0f, 0.0f, 0.0f };
        engineSceneSetGravityVector(scene_, gravity);

        // core
        auto camera_script = register_script<CameraScript>();
        auto ball_script = register_script<BallScript>();
        // players
        PlayerPaddleScript* left_player_script = nullptr;
        if (left_player_type == PlayerType::eHuman)
        {
            left_player_script = register_script<LeftPlayerPaddleScript>();
        }
        else
        {
            left_player_script = register_script<BotPlayerPaddleScript>();
        }
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


        auto ui_script = register_script<BattlegroundUiScript>();
        ui_script->set_left_player_script(left_player_script, left_player_type != PlayerType::eHuman);
        ui_script->set_right_player_script(right_player_script);
    }

    // deactivate at start
    deactivate();
}

void pong::BattlegroundScene::activate()
{
    //engineUiDocumentShow(ui_doc_);
    IScene::activate();
}


void pong::BattlegroundScene::deactivate()
{
    //engineUiDocumentHide(ui_doc_);
    IScene::deactivate();
}