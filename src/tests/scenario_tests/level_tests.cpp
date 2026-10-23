#include "scenario_harness.h"

#include <scenes/level_scene.h>
#include <scripts/encounter_script.h>
#include <scripts/enemy_script.h>
#include <scripts/gameplay_hud_script.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

// Smoke test of the real level (LevelScene loading assets/scenes/test_level.json). It renders the whole level every frame, so it is the slowest
// scenario per frame (~17 ms in Debug); keep it short.
namespace
{
using LevelTest = scenario::ScenarioTest;

TEST_F(LevelTest, LevelLoadsAndPlays)
{
    auto* scene = load_scene<project_c::LevelScene>();
    step(60);

    const auto* player = scene->find_script<project_c::Player>();
    ASSERT_NE(player, nullptr);
    EXPECT_FALSE(player->is_dead());
    EXPECT_NE(scene->find_script<project_c::GameplayHud>(), nullptr);
    // one pack of 5
    EXPECT_EQ(scene->get_scripts<project_c::Enemy>().size(), 5u);
    const auto* encounter = scene->find_script<project_c::Encounter>();
    ASSERT_NE(encounter, nullptr);
    EXPECT_EQ(encounter->get_state(), project_c::Encounter::State::eInProgress);
    EXPECT_EQ(encounter->get_remaining_enemies(), 5u);
}

// A scene destroyed before its first update still owns the scripts waiting in its register queue (the HUD's
// UI document among them); leaking them made the next load of the level crash.
TEST_F(LevelTest, LevelUnloadedBeforeFirstStepCanBeLoadedAgain)
{
    load_scene<project_c::LevelScene>();
    TearDown();

    auto* scene = load_scene<project_c::LevelScene>();
    step(1);
    EXPECT_NE(scene->find_script<project_c::GameplayHud>(), nullptr);
    EXPECT_NE(scene->find_script<project_c::Player>(), nullptr);
}
}  // namespace
