#pragma once
#include "scenario_harness.h"

#include <scenes/level_builder.h>

#include <iscene.h>

#include <glm/glm.hpp>

namespace project_c
{
class CameraScript;
class Encounter;
class Player;
}  // namespace project_c

namespace scenario
{
// A tiny gameplay scene for one scenario: camera, light and the encounter rules, no HUD. The test places what it
// needs with build(), the same LevelBuilder the level uses, e.g.
//   scene->build().floor_area(-3, -3, 3, 3);
//   scene->build().player(0, 0);
//   scene->build().enemy(0, 1);
// Everything is placed at exact positions, so runs are deterministic (no random spawners). The nav mesh is built from
// what was placed before the first step.
class SandboxScene : public engine::IScene
{
public:
    SandboxScene(engine::IApplication* app);
    static constexpr const char* get_name() { return "scenario_sandbox_scene"; }

    engine_result_code_t update(float dt) override;

    project_c::LevelBuilder& build() { return builder_; }
    // the camera follows the player with a spring; call after placing the player so it starts on it
    void snap_camera_to_player();

    project_c::Player* player();
    project_c::Encounter* encounter() { return encounter_; }

private:
    project_c::LevelBuilder builder_;
    project_c::CameraScript* camera_ = nullptr;
    project_c::Encounter* encounter_ = nullptr;
    bool nav_mesh_built_ = false;
};

class GameplayTest : public ScenarioTest
{
protected:
    SandboxScene* load_sandbox_scene() { return load_scene<SandboxScene>(); }

    // Steps `frames` frames with `input`, pointing the cursor at the world position `target` before every frame
    // (the camera moves, so the screen position of a fixed point changes). The player faces the cursor.
    void step_aiming(std::uint32_t frames, project_c::PlayerInput input, const glm::vec3& target);

    static glm::vec3 position_of(engine_game_object_t go);
};
}  // namespace scenario
