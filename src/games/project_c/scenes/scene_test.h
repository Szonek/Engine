#pragma once

#include "engine.h"
#include "iscene.h"

namespace project_c
{
class GameplayHud;

// The level: built from an ASCII map, with an Encounter (win/lose rules) and the GameplayHud.
// Gameplay scripts don't depend on it; it only adds the level rules (restart, HUD visibility).
class TestScene : public engine::IScene
{
public:
    TestScene(engine::IApplication* app);

    void activate() override;
    void deactivate() override;

    // the level's per-frame rules on top of IScene::update; runs also while the editor is paused
    engine_result_code_t update(float dt) override;
    static constexpr const char* get_name() { return "TestScene"; }

private:
    // the HUD by game object, its script looked up on use (its script component can be removed in the editor)
    GameplayHud* hud();
    engine_game_object_t hud_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    bool restart_was_down_ = false;
};

}// namespace project_c
