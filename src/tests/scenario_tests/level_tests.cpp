#include "scenario_harness.h"

#include <scenes/level_scene.h>
#include <scripts/encounter_script.h>
#include <scripts/enemy_script.h>
#include <scripts/game_components.h>
#include <scripts/gameplay_hud_script.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

// Smoke test of the real level (LevelScene loading assets/scenes/test_level.json). It renders the whole level every frame, so it is the slowest
// scenario per frame (~17 ms in Debug); keep it short.
namespace
{
using LevelTest = scenario::ScenarioTest;

const std::filesystem::path LEVEL_FILE = std::filesystem::path(TEST_ASSETS_DIR) / "scenes" / "test_level.json";

// the file's text with the line ends the engine writes (a checkout may have turned them into CRLF)
std::string read_text(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::erase(text, '\r');
    return text;
}

// a level file in the temp folder, loaded instead of the game's level while it lives
class TempLevel
{
public:
    explicit TempLevel(const std::string& content)
    {
        std::ofstream(path_, std::ios::binary) << content;
        scenario::app().set_level_path(path_.string());
    }
    ~TempLevel()
    {
        scenario::app().set_level_path(project_c::AppConfig{}.level_path);
        std::filesystem::remove(path_);
    }

private:
    std::filesystem::path path_ = std::filesystem::temp_directory_path() / "project_c_level_test.json";
};

// replaces the first `from` in `text`; the test fails when there is none
void replace_first(std::string& text, const std::string& from, const std::string& to)
{
    const auto at = text.find(from);
    ASSERT_NE(at, std::string::npos) << from;
    text.replace(at, from.size(), to);
}

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

// the level holds a player start, not the player: the player is spawned there, as a runtime object
TEST_F(LevelTest, PlayerIsSpawnedAtThePlayerStart)
{
    auto* scene = load_scene<project_c::LevelScene>();
    step(1);
    const auto* player = scene->find_script<project_c::Player>();
    ASSERT_NE(player, nullptr);
    EXPECT_TRUE(engineGameObjectIsTransient(player->get_game_object()));
    const auto text = read_text(LEVEL_FILE);
    EXPECT_EQ(text.find("\"player\""), std::string::npos);
    EXPECT_NE(text.find("\"player_start\": {"), std::string::npos);
}

// what is loaded saves back to the same file: nothing is lost or added on the way (the spawned player isn't saved)
TEST_F(LevelTest, LevelFileSavesBackToTheSameFile)
{
    load_scene<project_c::LevelScene>();
    const auto saved = std::filesystem::temp_directory_path() / "project_c_level_resaved.json";
    ASSERT_EQ(engineSceneSave(saved.string().c_str()), ENGINE_RESULT_CODE_OK);
    const auto again = read_text(saved);
    std::filesystem::remove(saved);
    EXPECT_EQ(again, read_text(LEVEL_FILE));
}

// a file edited by hand or saved by another version: unknown components and fields are skipped (logged), missing
// fields keep their defaults, and the level still plays
TEST_F(LevelTest, LevelFileWithUnknownAndMissingDataStillPlays)
{
    auto text = read_text(LEVEL_FILE);
    replace_first(text, "\"components\": {", "\"components\": { \"no_such_component\": { \"x\": 1 },");
    replace_first(text, "\"max_hp\": 20,", "\"no_such_field\": 1,");
    const TempLevel level(text);

    auto* scene = load_scene<project_c::LevelScene>();
    step(60);
    ASSERT_NE(scene->find_script<project_c::Player>(), nullptr);
    const auto enemies = scene->get_scripts<project_c::Enemy>();
    ASSERT_EQ(enemies.size(), 5u);
    // the enemy whose max_hp was taken out has the default
    for (const auto* enemy : enemies)
    {
        EXPECT_EQ(enemy->get_max_hp(), project_c::default_enemy_config().max_hp);
    }
    EXPECT_EQ(scene->find_script<project_c::Encounter>()->get_state(), project_c::Encounter::State::eInProgress);
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
