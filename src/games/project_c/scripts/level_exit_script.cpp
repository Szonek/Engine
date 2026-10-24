#include "level_exit_script.h"

#include "player_script.h"
#include "../app.h"

#include <iscene.h>
#include <utils.h>

#include <cstring>
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
    taken_ = true;
    const auto exit = config();
    if (exit.level[0] == '\0')
    {
        log("A level exit without a level was entered; it leads nowhere.\n");
        return;
    }
    auto* app = static_cast<AppProjectC*>(my_scene_->get_app());
    // the arrays are full strings when the file's text fills them
    app->request_level_travel(LevelTravel{
        std::string(exit.level, strnlen(exit.level, sizeof(exit.level))),
        std::string(exit.entrance, strnlen(exit.entrance, sizeof(exit.entrance))),
        player->get_state() });
}
