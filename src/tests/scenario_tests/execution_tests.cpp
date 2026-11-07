#include "sandbox_scene.h"

#include <app.h>
#include <scripts/building_script.h>
#include <scripts/enemy_script.h>
#include <scripts/enemy_spawner_script.h>
#include <scripts/game_components.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

// The player's executions (EXECUTIONS.md): from behind an enemy which hasn't noticed it, from above (the plunge) and
// in a chain. Outcomes (who died, who noticed, the movement mode the player went through) with generous time limits.
namespace
{
using project_c::Awareness;
using project_c::Enemy;
using project_c::MovementMode;
using project_c::Player;
using project_c::PlayerInput;

// frames for the player and the enemies, spawned above the floor, to land
constexpr std::uint32_t SETTLE_FRAMES = 45;
// an execution takes 0.55 s, with its hit stop
constexpr std::uint32_t EXECUTION_FRAMES = 60;
// yaw (around +y): facing +z, -z, +x
constexpr float FACING_PLUS_Z = 0.0f;
constexpr float FACING_MINUS_Z = 180.0f;
constexpr float FACING_PLUS_X = 90.0f;
// the block the plunge tests jump off
constexpr float BLOCK_HEIGHT = 3.0f;
constexpr float BLOCK_EDGE_X = 2.0f;

class ExecutionTest : public scenario::GameplayTest
{
protected:
    scenario::SandboxScene* sandbox(float half_size = 7.0f)
    {
        scene_ = load_sandbox_scene();
        scene_->build().floor_area(-half_size, -half_size, half_size, half_size);
        return scene_;
    }

    // one frame with attack pressed, then `frames` without it; every movement mode the player went through is in
    // modes_seen_
    void press_attack(std::uint32_t frames = EXECUTION_FRAMES)
    {
        PlayerInput attack{};
        attack.attack = true;
        set_input(attack);
        step_seeing();
        set_input({});
        for (std::uint32_t frame = 0; frame < frames; frame++)
        {
            step_seeing();
        }
    }

    void step_seeing()
    {
        step();
        if (auto* player = scene_->player())
        {
            modes_seen_.push_back(player->movement_mode());
        }
    }

    bool went_through(MovementMode mode) const
    {
        return std::find(modes_seen_.begin(), modes_seen_.end(), mode) != modes_seen_.end();
    }

    // dead, dying or gone
    bool is_dead(engine_game_object_t enemy) const
    {
        const auto* script = scene_->get_script<Enemy>(enemy);
        return !script || script->is_dying();
    }

    static void place(engine_game_object_t go, float x, float z, float yaw_degrees)
    {
        auto tc = engineGameObjectGetTransformComponent(go);
        tc.position[0] = x;
        tc.position[2] = z;
        const auto rotation = glm::angleAxis(glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
        tc.rotation[0] = rotation.x;
        tc.rotation[1] = rotation.y;
        tc.rotation[2] = rotation.z;
        tc.rotation[3] = rotation.w;
        engineGameObjectUpdateTransformComponent(go, &tc);
    }

    // a block BLOCK_HEIGHT tall around the origin (its east edge at BLOCK_EDGE_X) with the player on it
    Player* player_on_block()
    {
        sandbox();
        scene_->build().obstacle(glm::vec3(0.0f, BLOCK_HEIGHT * 0.5f, 0.0f), glm::vec3(BLOCK_EDGE_X * 2.0f, BLOCK_HEIGHT, BLOCK_EDGE_X * 2.0f));
        auto* player = scene_->build().player(0.0f, 0.0f);
        auto tc = engineGameObjectGetTransformComponent(player->get_game_object());
        tc.position[1] = BLOCK_HEIGHT + 0.05f;
        engineGameObjectUpdateTransformComponent(player->get_game_object(), &tc);
        scene_->snap_camera_to_player();
        return player;
    }

    // with `input`, steps until `done` (checked after every frame); false when it isn't done within `max_frames`
    bool step_until(const PlayerInput& input, std::uint32_t max_frames, const std::function<bool()>& done)
    {
        set_input(input);
        for (std::uint32_t frame = 0; frame < max_frames; frame++)
        {
            step_seeing();
            if (done())
            {
                return true;
            }
        }
        return false;
    }

    // a data asset of `type` holding `value_json`, from a temporary file; release it with engineDataAssetRelease
    static engine_data_asset_t temporary_data_asset(const char* type, const char* value_json)
    {
        const auto file = std::filesystem::temp_directory_path() / (std::string("project_c_scenario_") + type + ".json");
        std::ofstream(file, std::ios::binary) << "{ \"version\": 1, \"type\": \"" << type << "\", \"value\": " << value_json << " }";
        const auto asset = engineDataAssetLoad(file.generic_string().c_str());
        std::filesystem::remove(file);
        return asset;
    }

    // runs off the block's east edge until the plunge has a target, then presses attack and steps until the player is
    // on the ground; false when it never got a target or never landed
    bool plunge_off_block(Player* player)
    {
        if (!step_until(moving_right(), 120, [&] { return player->execution_target() != ENGINE_INVALID_GAME_OBJECT_ID; }))
        {
            return false;
        }
        PlayerInput attack{};
        attack.attack = true;
        attack.right = true;
        set_input(attack);
        step_seeing();
        return step_until({}, 60, [&] { return player->movement_mode() == MovementMode::eGrounded; });
    }

    static PlayerInput moving_right()
    {
        PlayerInput input{};
        input.right = true;
        return input;
    }

    scenario::SandboxScene* scene_ = nullptr;
    std::vector<MovementMode> modes_seen_;
};
}  // namespace

// behind an unaware enemy, attack executes it: it dies, silently for its pack 5 m away
TEST_F(ExecutionTest, ExecutionFromBehindKills)
{
    auto* scene = sandbox();
    auto config = project_c::default_enemy_spawner();
    config.enemy_type = scenario::app().get_skeleton_warrior_type();
    config.count = 2;
    config.area_half_extents[0] = 1.0f;
    config.area_half_extents[1] = 1.0f;
    auto* spawner = scene->build().enemy_spawner(0.0f, 0.0f, config);
    auto* player = scene->build().player(0.0f, -1.0f);
    ASSERT_NE(player, nullptr);
    scene->snap_camera_to_player();
    step(2);
    ASSERT_EQ(spawner->get_spawned().size(), 2u);
    const auto victim = spawner->get_spawned()[0];
    const auto packmate = spawner->get_spawned()[1];
    // the victim faces away from the player, its packmate 5 m off, facing away from both
    place(victim, 0.0f, 0.0f, FACING_PLUS_Z);
    place(packmate, 5.0f, 0.0f, FACING_PLUS_X);
    step(SETTLE_FRAMES);
    ASSERT_EQ(player->execution_target(), victim);

    press_attack();
    EXPECT_TRUE(went_through(MovementMode::eExecuting));
    EXPECT_TRUE(is_dead(victim));
    EXPECT_EQ(player->movement_mode(), MovementMode::eGrounded);
    // a silent kill: its packmate notices nothing
    step(60);
    ASSERT_NE(scene->get_script<Enemy>(packmate), nullptr);
    EXPECT_EQ(scene->get_script<Enemy>(packmate)->awareness().state(), Awareness::eUnaware);
}

// facing the player it sees it: the press is a normal attack
TEST_F(ExecutionTest, AttackFromTheFrontIsNotAnExecution)
{
    auto* scene = sandbox();
    scene->build().player(0.0f, 0.0f);
    auto* enemy = scene->build().enemy(0.0f, 1.0f, FACING_MINUS_Z);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);

    press_attack(10);
    EXPECT_FALSE(went_through(MovementMode::eExecuting));
}

TEST_F(ExecutionTest, AlertEnemyIsNotExecuted)
{
    auto* scene = sandbox();
    auto* player = scene->build().player(0.0f, -1.0f);
    auto* enemy = scene->build().enemy(0.0f, 0.0f, FACING_PLUS_Z);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);
    enemy->receive_attack_hit(0, 0.0f);
    // the player looks for its target before or after the enemy updates
    step(2);
    ASSERT_EQ(enemy->awareness().state(), Awareness::eAlert);
    EXPECT_EQ(player->execution_target(), ENGINE_INVALID_GAME_OBJECT_ID);

    press_attack(10);
    EXPECT_FALSE(went_through(MovementMode::eExecuting));
}

TEST_F(ExecutionTest, NoExecutionThroughAWall)
{
    auto* scene = sandbox();
    // a thin wall between them, the enemy facing away from it
    scene->build().obstacle(glm::vec3(0.0f, 0.75f, -0.6f), glm::vec3(2.0f, 1.5f, 0.1f));
    auto* player = scene->build().player(0.0f, -1.2f);
    auto* enemy = scene->build().enemy(0.0f, 0.0f, FACING_PLUS_Z);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);
    EXPECT_EQ(player->execution_target(), ENGINE_INVALID_GAME_OBJECT_ID);

    press_attack(10);
    EXPECT_FALSE(went_through(MovementMode::eExecuting));
    EXPECT_FALSE(is_dead(enemy->get_game_object()));
}

TEST_F(ExecutionTest, PlayerCannotBeHurtWhileExecuting)
{
    auto* scene = sandbox();
    auto* player = scene->build().player(0.0f, -1.0f);
    auto* enemy = scene->build().enemy(0.0f, 0.0f, FACING_PLUS_Z);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);

    PlayerInput attack{};
    attack.attack = true;
    set_input(attack);
    step();
    set_input({});
    ASSERT_EQ(player->movement_mode(), MovementMode::eExecuting);
    const auto hp = player->get_hp();
    EXPECT_FALSE(player->receive_damage(10, glm::vec3(1.0f, 0.0f, -1.0f)));
    EXPECT_EQ(player->get_hp(), hp);
}

// unarmed (the weapon dropped) the player executes too
TEST_F(ExecutionTest, ExecutionWorksUnarmed)
{
    auto* scene = sandbox();
    auto* player = scene->build().player(0.0f, -1.0f);
    auto* enemy = scene->build().enemy(0.0f, 0.0f, FACING_PLUS_Z);
    ASSERT_NE(enemy, nullptr);
    const auto victim = enemy->get_game_object();
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);
    PlayerInput drop{};
    drop.drop_weapon = true;
    set_input(drop);
    step();
    set_input({});
    step(SETTLE_FRAMES);
    ASSERT_EQ(player->state().weapon, project_c::WeaponType::eNone);

    press_attack();
    EXPECT_TRUE(went_through(MovementMode::eExecuting));
    EXPECT_TRUE(is_dead(victim));
}

// after an execution, a press in the chain window dashes to the next unnoticing enemy within the chain's range
TEST_F(ExecutionTest, ChainReachesTheNextEnemy)
{
    auto* scene = sandbox();
    scene->build().player(0.0f, -1.0f);
    auto* first = scene->build().enemy(0.0f, 0.0f, FACING_PLUS_Z);
    auto* second = scene->build().enemy(3.0f, 0.0f, FACING_PLUS_Z);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    const auto first_go = first->get_game_object();
    const auto second_go = second->get_game_object();
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);

    press_attack();
    ASSERT_TRUE(is_dead(first_go));
    ASSERT_FALSE(is_dead(second_go));
    press_attack();
    EXPECT_TRUE(is_dead(second_go));
}

// after the window, the next enemy (out of the ground execution's range) isn't reached
TEST_F(ExecutionTest, ChainWindowExpires)
{
    auto* scene = sandbox();
    scene->build().player(0.0f, -1.0f);
    auto* first = scene->build().enemy(0.0f, 0.0f, FACING_PLUS_Z);
    auto* second = scene->build().enemy(3.0f, 0.0f, FACING_PLUS_Z);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    const auto second_go = second->get_game_object();
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);

    press_attack();
    const auto window_frames = static_cast<std::uint32_t>(project_c::default_execution_tuning().chain_window_seconds * 60.0f) + 10;
    step(window_frames);
    modes_seen_.clear();
    press_attack();
    EXPECT_FALSE(went_through(MovementMode::eExecuting));
    EXPECT_FALSE(is_dead(second_go));
}

// an enemy type whose execution doesn't kill takes the execution's damage and is alerted
TEST_F(ExecutionTest, ExecutionKillsEnemiesOnlyWhenTheTypeSaysSo)
{
    auto* scene = sandbox();
    scene->build().player(0.0f, -1.0f);
    auto* enemy = scene->build().enemy(0.0f, 0.0f, FACING_PLUS_Z);
    ASSERT_NE(enemy, nullptr);
    const auto go = enemy->get_game_object();
    const auto file = std::filesystem::temp_directory_path() / "project_c_scenario_heavy_enemy.json";
    std::ofstream(file, std::ios::binary) << R"({ "version": 1, "type": "enemy_type", "value": { "max_hp": 100, "execution_kills": false } })";
    const auto heavy = engineDataAssetLoad(file.generic_string().c_str());
    std::filesystem::remove(file);
    ASSERT_NE(heavy, ENGINE_INVALID_OBJECT_HANDLE);
    const auto type = project_c::game_components().enemy_config;
    auto config = project_c::get_game_component(go, type, project_c::default_enemy_config());
    config.type = heavy;
    ASSERT_EQ(engineGameObjectUpdateDataComponent(go, type, &config), ENGINE_RESULT_CODE_OK);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);
    ASSERT_EQ(enemy->hp, 100u);

    press_attack(30);
    EXPECT_TRUE(went_through(MovementMode::eExecuting));
    EXPECT_EQ(enemy->hp, 100u - static_cast<std::uint32_t>(project_c::default_execution_tuning().execution_damage));
    EXPECT_EQ(enemy->awareness().state(), Awareness::eAlert);
    engineDataAssetRelease(heavy);
}

// running off a block over an unaware pack, attack in the air plunges onto one: it dies, the others around it are
// knocked down and then alerted, and the landing is loud
TEST_F(ExecutionTest, PlungeOntoUnawarePackExecutesOneAndKnocksDownTheRest)
{
    auto* player = player_on_block();
    auto* victim = scene_->build().enemy(5.0f, 0.0f, FACING_PLUS_X);
    auto* left = scene_->build().enemy(5.0f, 1.0f, FACING_PLUS_X);
    auto* right = scene_->build().enemy(5.0f, -1.0f, FACING_PLUS_X);
    ASSERT_NE(victim, nullptr);
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    step(SETTLE_FRAMES);

    // off the edge, until the plunge has a target
    ASSERT_TRUE(step_until(moving_right(), 120, [&] { return player->execution_target() != ENGINE_INVALID_GAME_OBJECT_ID; }));
    EXPECT_EQ(player->movement_mode(), MovementMode::eAirborne);
    EXPECT_GT(player->plunge_knockdown_radius(), 1.0f);
    const auto target = player->execution_target();
    std::vector<Enemy*> others;
    for (auto* enemy : { victim, left, right })
    {
        if (enemy->get_game_object() != target)
        {
            others.push_back(enemy);
        }
    }
    ASSERT_EQ(others.size(), 2u);

    PlayerInput attack{};
    attack.attack = true;
    attack.right = true;
    set_input(attack);
    step_seeing();
    ASSERT_TRUE(step_until({}, 60, [&] { return player->movement_mode() == MovementMode::eGrounded; }));
    EXPECT_TRUE(went_through(MovementMode::eExecuting));
    EXPECT_TRUE(is_dead(target));
    EXPECT_TRUE(player->noise_is_loud());
    for (auto* other : others)
    {
        EXPECT_TRUE(other->is_knocked_down());
    }
    // invulnerable through the landing's recovery
    const auto hp = player->get_hp();
    EXPECT_FALSE(player->receive_damage(10, glm::vec3(1.0f, 0.0f, 0.0f)));
    EXPECT_EQ(player->get_hp(), hp);

    // they are alerted (and search once they lose the player) and get up
    std::vector<bool> alerted(others.size(), false);
    const auto knockdown_frames = static_cast<std::uint32_t>(project_c::default_execution_tuning().knockdown_seconds * 60.0f) + 30;
    for (std::uint32_t frame = 0; frame < knockdown_frames; frame++)
    {
        step();
        for (std::size_t i = 0; i < others.size(); i++)
        {
            alerted[i] = alerted[i] || others[i]->awareness().state() == Awareness::eAlert;
        }
    }
    for (std::size_t i = 0; i < others.size(); i++)
    {
        EXPECT_FALSE(others[i]->is_knocked_down());
        EXPECT_TRUE(alerted[i]);
    }
}

// with nobody below, the press in the air waits for the landing, which is a normal one
TEST_F(ExecutionTest, PlungeWithoutTargetLandsNormally)
{
    auto* player = player_on_block();
    auto* far_enemy = scene_->build().enemy(-5.0f, -5.0f, FACING_MINUS_Z);
    ASSERT_NE(far_enemy, nullptr);
    step(SETTLE_FRAMES);

    ASSERT_TRUE(step_until(moving_right(), 120, [&] { return player->movement_mode() == MovementMode::eAirborne; }));
    step_seeing();
    EXPECT_EQ(player->execution_target(), ENGINE_INVALID_GAME_OBJECT_ID);
    PlayerInput attack{};
    attack.attack = true;
    set_input(attack);
    step_seeing();
    EXPECT_EQ(player->movement_mode(), MovementMode::eAirborne);
    ASSERT_TRUE(step_until({}, 120, [&] { return player->movement_mode() == MovementMode::eGrounded; }));
    EXPECT_FALSE(went_through(MovementMode::eExecuting));
    EXPECT_FALSE(is_dead(far_enemy->get_game_object()));
}

// with no landing recovery the plunge's landing still ends: the player can be hurt again and the chain window opens
TEST_F(ExecutionTest, PlungeLandingWithoutRecoveryEndsInvulnerability)
{
    auto* player = player_on_block();
    const auto traversal = temporary_data_asset("traversal_tuning", R"({ "landing_recovery_seconds": 0 })");
    ASSERT_NE(traversal, ENGINE_INVALID_OBJECT_HANDLE);
    const auto type = project_c::game_components().player_config;
    auto config = project_c::get_game_component(player->get_game_object(), type, project_c::default_player_config());
    config.traversal = traversal;
    ASSERT_EQ(engineGameObjectUpdateDataComponent(player->get_game_object(), type, &config), ENGINE_RESULT_CODE_OK);
    auto* enemy = scene_->build().enemy(5.0f, 0.0f, FACING_PLUS_X);
    ASSERT_NE(enemy, nullptr);
    step(SETTLE_FRAMES);

    ASSERT_TRUE(plunge_off_block(player));
    EXPECT_TRUE(went_through(MovementMode::eExecuting));
    EXPECT_TRUE(is_dead(enemy->get_game_object()));
    EXPECT_TRUE(player->is_chain_window_open());
    step(2);
    const auto hp = player->get_hp();
    EXPECT_TRUE(player->receive_damage(1, glm::vec3(10.0f, 0.0f, 0.0f)));
    EXPECT_LT(player->get_hp(), hp);
    engineDataAssetRelease(traversal);
}

// the chain window counts down off the ground too: a fall right after an execution doesn't keep it open
TEST_F(ExecutionTest, ChainWindowClosesDuringAFall)
{
    sandbox();
    constexpr float HIGH_BLOCK = 6.0f;
    scene_->build().obstacle(glm::vec3(0.0f, HIGH_BLOCK * 0.5f, 0.0f), glm::vec3(BLOCK_EDGE_X * 2.0f, HIGH_BLOCK, BLOCK_EDGE_X * 2.0f));
    auto* player = scene_->build().player(1.5f, 0.0f);
    // on the block, facing away from the player (toward -x)
    auto* enemy = scene_->build().enemy(0.5f, 0.0f, -FACING_PLUS_X);
    ASSERT_NE(enemy, nullptr);
    for (const auto go : { player->get_game_object(), enemy->get_game_object() })
    {
        auto tc = engineGameObjectGetTransformComponent(go);
        tc.position[1] = HIGH_BLOCK + 0.05f;
        engineGameObjectUpdateTransformComponent(go, &tc);
    }
    scene_->snap_camera_to_player();
    step(SETTLE_FRAMES);

    // the execution, until the player is back on its feet
    press_attack(1);
    ASSERT_TRUE(step_until({}, EXECUTION_FRAMES, [&] { return player->movement_mode() == MovementMode::eGrounded; }));
    ASSERT_TRUE(is_dead(enemy->get_game_object()));
    ASSERT_TRUE(player->is_chain_window_open());
    // off the edge right away, down 6 m
    ASSERT_TRUE(step_until(moving_right(), 240, [&]
        {
            return went_through(MovementMode::eAirborne) && player->movement_mode() == MovementMode::eGrounded;
        }));
    EXPECT_FALSE(player->is_chain_window_open());
}

// climbing, nothing is marked for execution (an attack press executes nobody there)
TEST_F(ExecutionTest, NothingIsMarkedWhileClimbing)
{
    sandbox();
    scene_->build().house(0.0f, 0.0f, 0.0f);
    const float wall_face_x = project_c::House::half_size + project_c::House::wall_thickness * 0.5f;
    const float wall_z = project_c::House::half_size * 0.5f;
    auto* player = scene_->build().player(wall_face_x + 0.3f, wall_z);
    // next to the wall, its back to the player
    auto* enemy = scene_->build().enemy(wall_face_x + 0.6f, wall_z + 1.0f, FACING_PLUS_Z);
    ASSERT_NE(enemy, nullptr);
    scene_->snap_camera_to_player();
    step(SETTLE_FRAMES);
    ASSERT_EQ(player->execution_target(), enemy->get_game_object());

    // sneaking, so the enemy keeps not noticing the player
    PlayerInput left{};
    left.left = true;
    left.sneak = true;
    ASSERT_TRUE(step_until(left, 60, [&] { return player->movement_mode() == MovementMode::eClimbing; }));
    ASSERT_EQ(enemy->awareness().state(), project_c::Awareness::eUnaware);
    // up the wall (and over it onto the roof, a scripted move): never marked
    for (int frame = 0; frame < 20; frame++)
    {
        step_seeing();
        EXPECT_NE(player->movement_mode(), MovementMode::eGrounded);
        EXPECT_EQ(player->execution_target(), ENGINE_INVALID_GAME_OBJECT_ID);
    }
}
