#pragma once

#include "engine.h"
#include "iscene.h"

namespace project_c
{
class GameplayHud;

// A level: a scene file (AppProjectC::get_level_path, assets/scenes/test_level.json by default) with the nav mesh built
// from it and the player spawned at its player start. Everything else (the camera, the Encounter, the GameplayHud) is
// in the file. Gameplay scripts don't depend on it; it only adds the level rules (restart, HUD visibility).
class LevelScene : public engine::IScene
{
public:
    LevelScene(engine::IApplication* app);

    void activate() override;
    void deactivate() override;

    // the level's per-frame rules on top of IScene::update; runs also while the editor is paused
    engine_result_code_t update(float dt) override;
    static constexpr const char* get_name() { return "LevelScene"; }

private:
    // the HUD, from the file: nullptr until its script was created (the first update) or without one
    GameplayHud* hud();
    bool restart_was_down_ = false;
};

}// namespace project_c
