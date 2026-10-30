#include "sandbox_scene.h"

#include <scripts/building_script.h>
#include <scripts/enemy_script.h>
#include <scripts/player_script.h>
#include <scripts/scripts_utils.h>

#include <iscene.h>
#include <iscript.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

// The script lifecycle (IScene): constructors set up their own game object, on_start runs once for every new script,
// after all scripts registered in the same frame exist and before the first update. Objects spawned at runtime are
// transient: they have no persistent ID and are never saved.
namespace
{
using scenario::EmptyScene;
using Log = std::vector<std::string>;

// writes its lifecycle calls to a log owned by the test, so a script deleted by the scene can still be checked
class Probe : public engine::IScript
{
public:
    Probe(engine::IScene* scene, Log* log, std::string name) : IScript(scene), log_(log), name_(std::move(name)) {}
    void on_start() override { log_->push_back(name_ + ".start"); }
    void update(float) override { log_->push_back(name_ + ".update"); }

private:
    Log* log_;
    std::string name_;
};

// registers a probe when it starts, like a script spawning its runtime parts
class Spawner : public engine::IScript
{
public:
    Spawner(engine::IScene* scene, Log* log) : IScript(scene), log_(log) {}
    void on_start() override
    {
        log_->push_back("spawner.start");
        my_scene_->register_script<Probe>(log_, "spawned");
    }
    void update(float) override { log_->push_back("spawner.update"); }

private:
    Log* log_;
};

class Second;
class First : public engine::IScript
{
public:
    First(engine::IScene* scene) : IScript(scene) {}
    void on_start() override;
    Second* found = nullptr;
};

class Second : public engine::IScript
{
public:
    Second(engine::IScene* scene) : IScript(scene) {}
    void on_start() override { found = my_scene_->find_script<First>(); }
    First* found = nullptr;
};

void First::on_start()
{
    found = my_scene_->find_script<Second>();
}

std::size_t count(const Log& log, const std::string& entry)
{
    return static_cast<std::size_t>(std::count(log.begin(), log.end(), entry));
}

std::size_t index_of(const Log& log, const std::string& entry)
{
    return static_cast<std::size_t>(std::find(log.begin(), log.end(), entry) - log.begin());
}

using ScriptLifecycleTest = scenario::ScenarioTest;

TEST_F(ScriptLifecycleTest, ScriptsRegisteredInTheSameFrameFindEachOtherInOnStart)
{
    auto* scene = load_scene<EmptyScene>();
    auto* first = scene->register_script<First>();
    auto* second = scene->register_script<Second>();
    step();

    EXPECT_EQ(first->found, second);
    EXPECT_EQ(second->found, first);
}

TEST_F(ScriptLifecycleTest, OnStartRunsOnceInRegistrationOrderBeforeTheFirstUpdate)
{
    auto* scene = load_scene<EmptyScene>();
    Log log;
    scene->register_script<Probe>(&log, "a");
    scene->register_script<Probe>(&log, "b");
    step(3);

    ASSERT_GE(log.size(), 2u);
    EXPECT_EQ(log[0], "a.start");
    EXPECT_EQ(log[1], "b.start");
    EXPECT_EQ(count(log, "a.start"), 1u);
    EXPECT_EQ(count(log, "b.start"), 1u);
    EXPECT_EQ(count(log, "a.update"), 3u);
    EXPECT_EQ(count(log, "b.update"), 3u);
}

TEST_F(ScriptLifecycleTest, ScriptRegisteredInOnStartStartsInTheSameFrame)
{
    auto* scene = load_scene<EmptyScene>();
    Log log;
    scene->register_script<Spawner>(&log);
    step();

    // both started before anyone's update, and both updated in that frame
    ASSERT_EQ(count(log, "spawned.start"), 1u);
    EXPECT_LT(index_of(log, "spawner.start"), index_of(log, "spawned.start"));
    EXPECT_LT(index_of(log, "spawned.start"), index_of(log, "spawner.update"));
    EXPECT_LT(index_of(log, "spawned.start"), index_of(log, "spawned.update"));
    EXPECT_EQ(count(log, "spawner.update"), 1u);
    EXPECT_EQ(count(log, "spawned.update"), 1u);
}

TEST_F(ScriptLifecycleTest, ScriptWhoseGameObjectWasDestroyedBeforeStartingNeverStarts)
{
    auto* scene = load_scene<EmptyScene>();
    Log log;
    auto* probe = scene->register_script<Probe>(&log, "probe");
    engineGameObjectDestroy(probe->get_game_object());
    step(2);

    EXPECT_TRUE(log.empty());
}

TEST_F(ScriptLifecycleTest, TransientGameObjectHasNoPersistentId)
{
    load_scene<EmptyScene>();
    const auto transient = engineGameObjectCreateTransient();
    const auto persistent = engineGameObjectCreate();
    ASSERT_TRUE(engineGameObjectIsValid(transient));
    EXPECT_TRUE(engineGameObjectIsTransient(transient));
    EXPECT_FALSE(engineGameObjectIsTransient(persistent));
    EXPECT_FALSE(engineGameObjectIsTransient(ENGINE_INVALID_GAME_OBJECT_ID));
}

// The game's scripts follow the rules: the level places the authored objects, scripts spawn their runtime parts
// (transient) when they start.
class RuntimeObjectsTest : public scenario::GameplayTest
{
protected:
    static engine_game_object_t child_named(engine_game_object_t go, const char* name)
    {
        for (const auto child : project_c::utils::get_children(go))
        {
            if (std::string(engineGameObjectGetNameComponent(child).name) == name)
            {
                return child;
            }
        }
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
};

TEST_F(RuntimeObjectsTest, CharactersSpawnTheirPartsAsTransientObjects)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-3.0f, -3.0f, 3.0f, 3.0f);
    auto* player = scene->build().player(0.0f, -2.0f);
    auto* enemy = scene->build().enemy(0.0f, 2.0f);
    ASSERT_NE(player, nullptr);
    ASSERT_NE(enemy, nullptr);
    step();

    // placed by the level
    EXPECT_FALSE(engineGameObjectIsTransient(player->get_game_object()));
    EXPECT_FALSE(engineGameObjectIsTransient(enemy->get_game_object()));

    // spawned by the player: its weapon (with the second dagger) and its attack trigger
    auto* weapon = scene->find_script<project_c::Weapon>();
    ASSERT_NE(weapon, nullptr);
    EXPECT_FALSE(weapon->is_on_ground());
    EXPECT_TRUE(engineGameObjectIsTransient(weapon->get_game_object()));
    // held in the player's other hand
    const auto offhand = child_named(player->get_game_object(), "weapon-daggers-offhand");
    ASSERT_NE(offhand, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_TRUE(engineGameObjectIsTransient(offhand));
    const auto triggers = scene->get_scripts<project_c::AttackTrigger>();
    ASSERT_EQ(triggers.size(), 1u);
    EXPECT_TRUE(engineGameObjectIsTransient(triggers.front()->get_game_object()));

    // spawned by the enemy: its blade
    const auto blades = scene->get_scripts<project_c::EnemyWeapon>();
    ASSERT_EQ(blades.size(), 1u);
    EXPECT_TRUE(engineGameObjectIsTransient(blades.front()->get_game_object()));
    EXPECT_EQ(engineGameObjectGetParentComponent(blades.front()->get_game_object()).parent, enemy->get_game_object());
}

TEST_F(RuntimeObjectsTest, WeaponPlacedOnTheGroundIsLevelContent)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-1.0f, -1.0f, 1.0f, 1.0f);
    auto* weapon = scene->build().weapon_on_ground(0.0f, 0.0f);
    ASSERT_NE(weapon, nullptr);
    step();

    EXPECT_TRUE(weapon->is_on_ground());
    EXPECT_FALSE(engineGameObjectIsTransient(weapon->get_game_object()));
    // its second dagger is created by the script, so it lies next to it as a runtime object
    const auto offhand = child_named(weapon->get_game_object(), "weapon-daggers-offhand");
    ASSERT_NE(offhand, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_TRUE(engineGameObjectIsTransient(offhand));
}

// The house is level content: its parts are its children; it hides the roof and the camera facing wall while the
// player is inside
class HouseTest : public RuntimeObjectsTest
{
protected:
    static constexpr float GROUND_Y = 0.125f;

    static project_c::BuildingPart* part_named(engine::IScene& scene, const project_c::House& house, const char* name)
    {
        return scene.get_script<project_c::BuildingPart>(child_named(house.get_game_object(), name));
    }

    static bool mesh_hidden(const project_c::BuildingPart& part)
    {
        return engineGameObjectGetMeshComponent(part.get_game_object()).disable;
    }
};

TEST_F(HouseTest, PartsAreLevelContentUnderTheHouse)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-4.0f, -4.0f, 4.0f, 4.0f);
    auto* house = scene->build().house(0.0f, 0.0f, GROUND_Y);
    ASSERT_NE(house, nullptr);
    step();

    const auto children = project_c::utils::get_children(house->get_game_object());
    // 8 wall pieces (one with the window) + the roof; the 4 window frame blockers are under the window piece
    EXPECT_EQ(children.size(), 9u);
    for (const auto child : children)
    {
        EXPECT_NE(scene->get_script<project_c::BuildingPart>(child), nullptr);
        EXPECT_FALSE(engineGameObjectIsTransient(child));
    }
    const auto* window = part_named(*scene, *house, "house-window");
    ASSERT_NE(window, nullptr);
    const auto frames = project_c::utils::get_children(window->get_game_object());
    EXPECT_EQ(frames.size(), 4u);
    for (const auto frame : frames)
    {
        EXPECT_NE(scene->get_script<project_c::BuildingPart>(frame), nullptr);
        EXPECT_FALSE(engineGameObjectIsTransient(frame));
    }
}

TEST_F(HouseTest, WindowFramesMoveWithTheWindow)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-4.0f, -4.0f, 4.0f, 4.0f);
    auto* house = scene->build().house(0.0f, 0.0f, GROUND_Y);
    ASSERT_NE(house, nullptr);
    step();
    const auto* window = part_named(*scene, *house, "house-window");
    ASSERT_NE(window, nullptr);
    const auto frames = project_c::utils::get_children(window->get_game_object());
    ASSERT_FALSE(frames.empty());
    auto* frame = scene->get_script<project_c::BuildingPart>(frames.front());
    ASSERT_NE(frame, nullptr);
    // the frames are climbed from outside of the house only (the window is in the south wall, +z)
    EXPECT_TRUE(frame->is_climbable({ 0.0f, 0.0f, 1.0f }));
    EXPECT_FALSE(frame->is_climbable({ 0.0f, 0.0f, -1.0f }));

    // the designer moves the window piece: what the player climbs through moves with it
    auto tc = engineGameObjectGetTransformComponent(window->get_game_object());
    tc.position[0] += 3.0f;
    engineGameObjectUpdateTransformComponent(window->get_game_object(), &tc);
    step();
    const auto window_position = project_c::utils::get_world_position(window->get_game_object());
    const auto frame_position = project_c::utils::get_world_position(frames.front());
    EXPECT_NEAR(frame_position.x, window_position.x, 1e-4f);
    EXPECT_NEAR(frame_position.z, window_position.z, 1e-4f);
    ASSERT_TRUE(frame->get_alignment_point().has_value());
    EXPECT_NEAR(frame->get_alignment_point()->x, window_position.x, 1e-4f);
}

TEST_F(HouseTest, RoofAndCameraFacingWallHideWhilePlayerIsInside)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-4.0f, -6.0f, 4.0f, 4.0f);
    auto* house = scene->build().house(0.0f, 0.0f, GROUND_Y);
    auto* player = scene->build().player(0.0f, -5.0f);
    ASSERT_NE(house, nullptr);
    ASSERT_NE(player, nullptr);
    step(30);

    auto* roof = part_named(*scene, *house, "house-roof");
    auto* window = part_named(*scene, *house, "house-window");
    ASSERT_NE(roof, nullptr);
    ASSERT_NE(window, nullptr);
    EXPECT_FALSE(mesh_hidden(*roof));
    EXPECT_FALSE(mesh_hidden(*window));
    EXPECT_TRUE(engineGameObjectHasColliderComponent(roof->get_game_object()));

    player->set_world_position(0.0f, 1.0f, 0.0f);
    step(30);
    EXPECT_TRUE(mesh_hidden(*roof));
    EXPECT_TRUE(mesh_hidden(*window));
    // so mouse aiming hits the room's floor: the roof's collider is disabled
    ASSERT_TRUE(engineGameObjectHasColliderComponent(roof->get_game_object()));
    EXPECT_TRUE(engineGameObjectGetColliderComponent(roof->get_game_object()).disable);
    const float origin[3] = { 0.0f, 10.0f, 0.0f };
    const float down[3] = { 0.0f, -1.0f, 0.0f };
    engine_ray_hit_t hit{};
    ASSERT_TRUE(enginePhysicsRayCast(origin, down, 20.0f, nullptr, &hit));
    EXPECT_NE(hit.go, roof->get_game_object());
}
}  // namespace
