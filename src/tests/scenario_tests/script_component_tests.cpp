#include "sandbox_scene.h"

#include <scripts/building_script.h>
#include <scripts/encounter_script.h>
#include <scripts/enemy_script.h>
#include <scripts/enviorment_script.h>
#include <scripts/game_components.h>
#include <scripts/interactable_script.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <typeinfo>

// Scripts as data: the `script` component names a game object's script, scene files save it, and the scene creates,
// replaces and removes scripts when the components change. Designer-set values live in config components, which
// scripts read on every use.
namespace
{
using project_c::Enemy;
using project_c::Player;

// frames for the player, spawned 1 m above the floor, to land
constexpr std::uint32_t LANDING_FRAMES = 30;

// a scene loaded from a file: nothing but what the file holds; the nav mesh is built before the first step
class FileScene : public engine::IScene
{
public:
    FileScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "scenario_script_file_scene"; }

    engine_result_code_t update(float dt) override
    {
        if (!nav_mesh_built_)
        {
            nav_mesh_built_ = true;
            engineNavMeshBuild();
        }
        return IScene::update(dt);
    }

private:
    bool nav_mesh_built_ = false;
};

class ScriptComponentTest : public scenario::GameplayTest
{
protected:
    void TearDown() override
    {
        GameplayTest::TearDown();
        std::filesystem::remove(file_);
    }

    // the active scene saved to the test's file, which then replaces it in a FileScene
    FileScene* save_and_load_into_file_scene(const char* current_scene_name)
    {
        EXPECT_EQ(engineSceneSave(file_.string().c_str()), ENGINE_RESULT_CODE_OK);
        scenario::app().unregister_scene(current_scene_name);
        auto* scene = load_scene<FileScene>();
        EXPECT_EQ(engineSceneLoad(file_.string().c_str()), ENGINE_RESULT_CODE_OK);
        return scene;
    }

    FileScene* load_file_scene(const std::string& content)
    {
        std::ofstream(file_, std::ios::binary) << content;
        auto* scene = load_scene<FileScene>();
        EXPECT_EQ(engineSceneLoad(file_.string().c_str()), ENGINE_RESULT_CODE_OK);
        return scene;
    }

    static engine_component_type_t script_component() { return scenario::app().scripts().get_component_type(); }

    template<typename T>
    static std::int32_t script_index() { return scenario::app().scripts().find_index(typeid(T)); }

    std::filesystem::path file_ = std::filesystem::temp_directory_path() / "project_c_script_component_test.json";
};

TEST_F(ScriptComponentTest, LevelSavedAndLoadedPlaysTheSame)
{
    auto* sandbox = load_sandbox_scene();
    sandbox->build().floor_area(-3.0f, -3.0f, 4.0f, 4.0f);
    sandbox->build().player(1.0f, 1.0f);
    sandbox->build().enemy(1.0f, 2.5f);
    // the scripts exist and are saved before the level ever played
    auto* scene = save_and_load_into_file_scene(scenario::SandboxScene::get_name());
    step(1);

    auto* player = scene->find_script<Player>();
    ASSERT_NE(player, nullptr);
    ASSERT_EQ(scene->get_scripts<Enemy>().size(), 1u);
    EXPECT_EQ(scene->get_scripts<project_c::Floor>().size(), 64u);
    ASSERT_NE(scene->find_script<project_c::Encounter>(), nullptr);
    // on_start ran: the player spawned and equipped its starting weapon
    EXPECT_NE(scene->find_script<project_c::Weapon>(), nullptr);
    EXPECT_EQ(player->get_hp(), player->get_max_hp());

    // the loaded enemy finds the loaded player, walks to it and hits it
    step(LANDING_FRAMES + 240);
    EXPECT_LT(player->get_hp(), player->get_max_hp());
    EXPECT_FALSE(player->is_dead());
    EXPECT_EQ(scene->find_script<project_c::Encounter>()->get_state(), project_c::Encounter::State::eInProgress);
}

TEST_F(ScriptComponentTest, ConfigEditedWhilePlayingTakesEffect)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-3.0f, -3.0f, 4.0f, 4.0f);
    scene->build().player(1.0f, 1.0f);
    auto* enemy = scene->build().enemy(1.0f, 3.5f);
    ASSERT_NE(enemy, nullptr);
    const auto go = enemy->get_game_object();
    const auto type = project_c::game_components().enemy_config;

    // an enemy which can't move stays where it is
    auto config = project_c::get_game_component(go, type, project_c::default_enemy_config());
    const auto speed = config.move_speed;
    config.move_speed = 0.0f;
    ASSERT_EQ(engineGameObjectUpdateDataComponent(go, type, &config), ENGINE_RESULT_CODE_OK);
    step(LANDING_FRAMES);
    const auto start = position_of(go);
    step(60);
    EXPECT_LT(glm::length(glm::vec2(position_of(go).x - start.x, position_of(go).z - start.z)), 0.05f);

    // the same enemy walks once its speed is back
    config.move_speed = speed;
    ASSERT_EQ(engineGameObjectUpdateDataComponent(go, type, &config), ENGINE_RESULT_CODE_OK);
    step(60);
    EXPECT_GT(glm::length(glm::vec2(position_of(go).x - start.x, position_of(go).z - start.z)), 0.3f);
}

TEST_F(ScriptComponentTest, ChangingTheScriptComponentReplacesTheScript)
{
    auto* scene = load_sandbox_scene();
    auto* chest = scene->build().chest(0.0f, 0.1f, 0.0f);
    ASSERT_NE(chest, nullptr);
    const auto go = chest->get_game_object();
    step(1);
    // a game object has one script
    EXPECT_EQ(scene->attach_script<project_c::Floor>(go), nullptr);

    engine::ScriptComponent component{ script_index<project_c::Floor>() };
    ASSERT_EQ(engineGameObjectUpdateDataComponent(go, script_component(), &component), ENGINE_RESULT_CODE_OK);
    step(1);
    EXPECT_TRUE(engineGameObjectIsValid(go));
    EXPECT_EQ(scene->get_script<project_c::Chest>(go), nullptr);
    EXPECT_NE(scene->get_script<project_c::Floor>(go), nullptr);

    // without the component, the game object stays without a script
    engineGameObjectRemoveDataComponent(go, script_component());
    step(1);
    EXPECT_TRUE(engineGameObjectIsValid(go));
    EXPECT_EQ(scene->get_script<project_c::Floor>(go), nullptr);
}

TEST_F(ScriptComponentTest, ScriptComponentFromAFile)
{
    // an unknown script name, and a house without its house_config
    auto* scene = load_file_scene(R"({ "version": 1, "entities": [
        { "id": 1, "components": { "name": { "name": "mystery" }, "script": { "type": "no_such_script" } } },
        { "id": 2, "components": { "name": { "name": "house" }, "transform": { "position": [0, 0, 0] }, "script": { "type": "house" } } } ] })");
    step(1);

    engine_game_object_t mystery = ENGINE_INVALID_GAME_OBJECT_ID;
    engine_game_object_t house = ENGINE_INVALID_GAME_OBJECT_ID;
    for (engine_game_object_t go = 1; go < 64; go++)
    {
        if (engineGameObjectIsValid(go) && engineGameObjectHasNameComponent(go))
        {
            const std::string name = engineGameObjectGetNameComponent(go).name;
            if (name == "mystery")
            {
                mystery = go;
            }
            else if (name == "house")
            {
                house = go;
            }
        }
    }
    ASSERT_NE(mystery, ENGINE_INVALID_GAME_OBJECT_ID);
    ASSERT_NE(house, ENGINE_INVALID_GAME_OBJECT_ID);
    // the unknown name is logged by the loader and kept as no script
    engine::ScriptComponent component{ -1 };
    ASSERT_EQ(engineGameObjectGetDataComponent(mystery, script_component(), &component), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(component.type, engine::ScriptRegistry::NO_SCRIPT);
    EXPECT_EQ(scene->get_script<project_c::BaseNode>(mystery), nullptr);
    // the house got its script and the component the script requires
    EXPECT_NE(scene->get_script<project_c::House>(house), nullptr);
    EXPECT_TRUE(engineGameObjectHasDataComponent(house, project_c::game_components().house_config));
}
}  // namespace
