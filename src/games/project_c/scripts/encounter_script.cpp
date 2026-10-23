#include "encounter_script.h"
#include "enemy_script.h"
#include "player_script.h"

#include <iscene.h>

project_c::Encounter::Encounter(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go)
{
}

void project_c::Encounter::update(float dt)
{
    remaining_enemies_ = 0;
    for (const auto* enemy : my_scene_->get_scripts<Enemy>())
    {
        if (enemy->hp > 0)
        {
            remaining_enemies_++;
        }
    }
    had_enemies_ = had_enemies_ || remaining_enemies_ > 0;

    if (is_over())
    {
        return;
    }

    const auto* player = my_scene_->find_script<Player>();
    if (player && player->is_dead())
    {
        state_ = State::eDefeated;
    }
    else if (had_enemies_ && remaining_enemies_ == 0)
    {
        state_ = State::eCleared;
    }
}
