#pragma once

#include "engine.h"
#include "iscene.h"

namespace project_c
{
class GameplayHud;

// A level: a scene file (AppProjectC::get_level_path, assets/scenes/test_level.json by default) with the nav mesh built
// from it and, while playing, the player spawned at the player start of the entrance (AppProjectC::get_level_entrance)
// with what they carry (AppProjectC::player_state). Everything else (the camera, the Encounter, the GameplayHud, the level exits)
// is in the file. Gameplay scripts don't depend on it; it only adds the level rules (going through a level exit to the
// next level, restart, HUD visibility).
class LevelScene : public engine::IScene
{
public:
    // throws std::runtime_error when the level file can't be loaded
    LevelScene(engine::IApplication* app);

    void activate() override;
    void deactivate() override;

    // the level's per-frame rules on top of IScene::update; runs also while the editor is paused
    engine_result_code_t update(float dt) override;
    static constexpr const char* get_name() { return "LevelScene"; }

protected:
    // spawns the player at the player start of the entrance, like Unreal's GameMode at a PlayerStart: the edit state
    // never holds a player
    void on_play_begin() override;
    void on_play_end() override;

private:
    // the HUD's and the encounter's game objects, once their scripts exist (after the first update of a play session):
    // their scripts are then looked up by game object, never by scanning every script each frame
    void find_level_scripts();
    // the HUD, from the file: nullptr before the first update, in a level without one or when its script was removed
    GameplayHud* hud();

    bool level_scripts_found_ = false;
    engine_game_object_t hud_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    engine_game_object_t encounter_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
};

}// namespace project_c
