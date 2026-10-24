#pragma once
#include "base_script.h"
#include "game_components.h"

namespace project_c
{
// A walk-in way to another level (a door, a cave mouth, the edge of the map): the player walking into its trigger
// travels to the level and entrance its level_exit component names, carrying what they have (AppProjectC::
// request_level_travel). Level content: what it is made of is placed by the level (LevelBuilder::level_exit).
class LevelExit : public BaseNode
{
public:
    LevelExit(engine::IScene* my_scene, engine_game_object_t go);

    // its designer-set values (the level_exit component), read on every use
    LevelExitConfig config() const;

    void on_collision(const collision_t& info) override;

private:
    // the trigger reports the player every frame it overlaps; the level is left once
    bool taken_ = false;
    // an exit without a level is logged once, not every frame the player stands on it; its level can be set while
    // playing (the config is read on every use)
    bool warned_no_level_ = false;
};
}  // namespace project_c
