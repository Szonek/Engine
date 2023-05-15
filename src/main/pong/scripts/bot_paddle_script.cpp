#include "player_paddle_script.h"
#include "ball_script.h"

#include "iscene.h"
#include "utils.h"
#include "global_constants.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cassert>

pong::BotPlayerPaddleScript::BotPlayerPaddleScript(engine::IScene *my_scene)
        : PlayerPaddleScript(my_scene, -12.0f, 0.25f, "bot_player")
{
    set_difficulty(Difficulty::eEasy);
}

void pong::BotPlayerPaddleScript::update(float dt)
{
    const auto paddle_pos = engineSceneGetTransformComponent(my_scene_->get_handle(), go_);
    const auto ball_pos = ball_script_->get_current_position();
    ball_history_.push_back(ball_pos[1]);

    // initialize history buffer
    if(ball_history_.size() == 1)
    {
        for(int i = 0; i < 256; i++)
        {
            ball_history_.push_back(ball_pos[1]);
        }
    }


    float next_paddle_pos_y = 0.0f;
    switch(difficulty_)
    {
        case Difficulty::eEasy:
        {
            // by default get position from frame_idx -12
            std::size_t histy_length = 12;

            // if ball is much higher/lower (in y) then simulate worse reaction by taking further history postion
            if(std::abs(ball_pos[1] - paddle_pos.position[1]) >= 8)
            {
                histy_length = 16;
            }

            // get the ball position from the history
            if(ball_history_.size() > histy_length)
            {
                next_paddle_pos_y = ball_history_.at(ball_history_.size() - histy_length );
            }
            else
            {
                next_paddle_pos_y = ball_history_.back();
            }



            break;
        }

        case Difficulty::eGodLike:
        default:
            next_paddle_pos_y = ball_pos[1];
            break;
    }



    set_target_worldspace_position(next_paddle_pos_y);
    pong::PlayerPaddleScript::update(dt);


    ball_history_.pop_front();
}

void pong::BotPlayerPaddleScript::set_difficulty(pong::BotPlayerPaddleScript::Difficulty difficulty)
{
    difficulty_ = difficulty;
}
