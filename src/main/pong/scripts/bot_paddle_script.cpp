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

}

void pong::BotPlayerPaddleScript::update(float dt)
{
    const auto ball_pos = ball_script_->get_current_position();
    set_target_worldspace_position(ball_pos[1]);
    pong::PlayerPaddleScript::update(dt);
}