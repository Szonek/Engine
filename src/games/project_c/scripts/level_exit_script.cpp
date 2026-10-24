#include "level_exit_script.h"

#include "player_script.h"
#include "scripts_utils.h"
#include "../app.h"

#include <iscene.h>
#include <utils.h>

#include <string>

project_c::LevelExit::LevelExit(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go)
{
}

project_c::LevelExitConfig project_c::LevelExit::config() const
{
    return get_game_component(go_, game_components().level_exit, default_level_exit());
}

void project_c::LevelExit::on_collision(const collision_t& info)
{
    const auto* player = my_scene_->get_script<Player>(info.other);
    if (taken_ || !player || player->is_dead())
    {
        return;
    }
    const auto exit = config();
    const auto level = utils::to_string_view(exit.level);
    if (level.empty())
    {
        if (!warned_no_level_)
        {
            log("A level exit without a level was entered; it leads nowhere.\n");
            warned_no_level_ = true;
        }
        return;
    }
    auto* app = static_cast<AppProjectC*>(my_scene_->get_app());
    // taken only when the travel was accepted (another exit's request may have come first this frame)
    taken_ = app->request_level_travel(LevelTravel{ std::string(level), std::string(utils::to_string_view(exit.entrance)) });
}
