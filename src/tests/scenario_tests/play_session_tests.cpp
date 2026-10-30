#include "scenario_harness.h"

#include <scenes/level_builder.h>
#include <scenes/level_scene.h>
#include <scripts/interactable_script.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

// Play sessions in the toolkit and the game: scripts exist only while playing, Play creates them from their
// components, Stop drops them and gives back the edit state, the level spawns the player only while playing.
namespace
{
// the scene the play session the next tests run in is started in
class NextTestsScene : public engine::IScene
{
public:
    NextTestsScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "play_session_test_next"; }
};

// A scene to build in Edit mode, like the editor's
class EditScene : public engine::IScene
{
public:
    EditScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "play_session_test_edit"; }
};

class PlaySessionTest : public scenario::ScenarioTest
{
protected:
    void SetUp() override
    {
        // the app starts playing; these tests start in Edit mode, as the editor does. Stop loads the (empty) scene the
        // last session was started in into the active scene: one which goes with it
        scenario::app().register_scene<NextTestsScene>()->activate();
        ASSERT_EQ(enginePlayModeExit(), ENGINE_RESULT_CODE_OK);
        scenario::app().unregister_scene(NextTestsScene::get_name());
    }

    void TearDown() override
    {
        // this test's session is thrown away: its snapshot would be loaded into the next test's scene by its Stop
        if (enginePlayModeGetState() != ENGINE_PLAY_MODE_EDIT)
        {
            enginePlayModeExit();
        }
        std::filesystem::remove(file_);
        ScenarioTest::TearDown();
        // the next tests play, in a session started in an empty scene
        scenario::app().register_scene<NextTestsScene>()->activate();
        EXPECT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
        scenario::app().unregister_scene(NextTestsScene::get_name());
    }

    // the active scene as a scene file
    std::string saved()
    {
        EXPECT_EQ(engineSceneSave(file_.string().c_str()), ENGINE_RESULT_CODE_OK);
        std::ifstream file(file_, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

private:
    const std::filesystem::path file_ = std::filesystem::temp_directory_path() / "play_session_test.json";
};

TEST_F(PlaySessionTest, ScriptsExistOnlyWhilePlaying)
{
    auto* scene = load_scene<EditScene>();
    project_c::LevelBuilder builder(scenario::app(), *scene);
    // in Edit mode the script is data: the component is set, no script is created
    EXPECT_EQ(builder.chest(0.0f, 0.0f, 0.0f), nullptr);
    step(3);
    EXPECT_TRUE(scene->get_scripts<project_c::Chest>().empty());

    ASSERT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
    step();
    ASSERT_EQ(scene->get_scripts<project_c::Chest>().size(), 1u);

    ASSERT_EQ(enginePlayModeExit(), ENGINE_RESULT_CODE_OK);
    step();
    EXPECT_TRUE(scene->get_scripts<project_c::Chest>().empty());

    // the next session creates it again, from the chest loaded back by Stop
    ASSERT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
    step();
    EXPECT_EQ(scene->get_scripts<project_c::Chest>().size(), 1u);
}

TEST_F(PlaySessionTest, StopGivesBackTheLevelWithoutItsPlayer)
{
    auto* scene = load_scene<project_c::LevelScene>();
    step(2);
    // the edit state never holds a player
    EXPECT_EQ(scene->find_script<project_c::Player>(), nullptr);
    const auto edit_state = saved();
    const auto level = scenario::app().get_level_path();

    ASSERT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
    step();
    // the level spawned the player at its player start, the spawners their enemies
    auto* player = scene->find_script<project_c::Player>();
    ASSERT_NE(player, nullptr);
    set_input({ .forward = true, .right = true });
    step(60);
    release_all_input();
    // what a session changes outside the scene
    scenario::app().set_level_path("somewhere_else.json", "back_door");

    ASSERT_EQ(enginePlayModeExit(), ENGINE_RESULT_CODE_OK);
    step();
    EXPECT_EQ(scene->find_script<project_c::Player>(), nullptr);
    // saving after Stop gives what saving before Play gave: no player, no spawned enemy, nothing moved
    EXPECT_EQ(saved(), edit_state);
    EXPECT_EQ(scenario::app().get_level_path(), level);
    EXPECT_EQ(scenario::app().get_level_entrance(), "default");

    // and Play again spawns a player again
    ASSERT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
    step();
    EXPECT_NE(scene->find_script<project_c::Player>(), nullptr);
}

TEST_F(PlaySessionTest, StepPlaysOneFrameOfTheScripts)
{
    auto* scene = load_scene<project_c::LevelScene>();
    ASSERT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
    step();
    ASSERT_EQ(enginePlayModeSetPaused(true), ENGINE_RESULT_CODE_OK);
    auto* player = scene->find_script<project_c::Player>();
    ASSERT_NE(player, nullptr);
    const auto go = player->get_game_object();
    const auto position = [go]() {
        const auto tc = engineGameObjectGetTransformComponent(go);
        return std::array<float, 3>{ tc.position[0], tc.position[1], tc.position[2] };
    };
    // the player walks in a played frame only
    set_input({ .forward = true, .right = true });
    step(5);
    const auto paused_at = position();
    step(5);
    EXPECT_EQ(position(), paused_at);
    ASSERT_EQ(enginePlayModeStep(), ENGINE_RESULT_CODE_OK);
    step();
    const auto stepped_to = position();
    EXPECT_NE(stepped_to, paused_at);
    step(5);
    EXPECT_EQ(position(), stepped_to);
    release_all_input();
}
}  // namespace
