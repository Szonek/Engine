#include "sandbox_scene.h"

#include <app.h>
#include <scripts/enemy_script.h>
#include <scripts/enemy_spawner_script.h>
#include <scripts/game_components.h>
#include <scripts/player_script.h>
#include <scripts/smoke_cloud.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <functional>
#include <vector>

// The smoke bomb (SMOKE.md): the cloud hides the player from sight, blinds the enemies it pops on (they lose track of
// the player, can't act, can be executed from any side) and is a loud noise; the throw's cooldown and range. Outcomes
// with generous time limits.
namespace
{
using project_c::Awareness;
using project_c::Enemy;
using project_c::MovementMode;
using project_c::Player;
using project_c::PlayerInput;
using project_c::SmokeCloud;

constexpr std::uint32_t SETTLE_FRAMES = 45;
// yaw (around +y): facing +z, -z
constexpr float FACING_PLUS_Z = 0.0f;
constexpr float FACING_MINUS_Z = 180.0f;
constexpr float FACING_PLUS_X = 90.0f;

std::uint32_t frames(float seconds)
{
    return static_cast<std::uint32_t>(seconds * 1000.0f / scenario::FIXED_DT_MS);
}

class SmokeTest : public scenario::GameplayTest
{
protected:
    scenario::SandboxScene* sandbox(float half_size = 16.0f)
    {
        scene_ = load_sandbox_scene();
        scene_->build().floor_area(-half_size, -half_size, half_size, half_size);
        return scene_;
    }

    Player* place_player(float x, float z)
    {
        player_ = scene_->build().player(x, z);
        scene_->snap_camera_to_player();
        return player_;
    }

    // E with the cursor on `target` (on the floor), then the throw plays out until its cloud popped (or `max_frames`)
    void throw_smoke_at(const glm::vec3& target, std::uint32_t max_frames = 60)
    {
        PlayerInput smoke{};
        smoke.smoke = true;
        const auto clouds_before = scene_->get_scripts<SmokeCloud>().size();
        step_aiming(1, smoke, target);
        for (std::uint32_t frame = 0; frame < max_frames && scene_->get_scripts<SmokeCloud>().size() == clouds_before; frame++)
        {
            step_aiming(1, {}, target);
        }
        // the cloud starts (pops) at its first update
        step(1);
    }

    std::vector<SmokeCloud*> clouds() { return scene_->get_scripts<SmokeCloud>(); }

    // steps until `done` (checked after every frame); false when it isn't done within `max_frames`
    bool step_until(std::uint32_t max_frames, const std::function<bool()>& done)
    {
        set_input({});
        for (std::uint32_t frame = 0; frame < max_frames; frame++)
        {
            step();
            if (done())
            {
                return true;
            }
        }
        return false;
    }

    bool is_dead(engine_game_object_t enemy) const
    {
        const auto* script = scene_->get_script<Enemy>(enemy);
        return !script || script->is_dying();
    }

    scenario::SandboxScene* scene_ = nullptr;
    Player* player_ = nullptr;
};
}  // namespace

// an alert enemy chasing the player loses it when the bomb bursts on it: it searches the cloud instead of hunting
TEST_F(SmokeTest, AlertEnemyLosesThePlayerAndSearchesTheCloud)
{
    sandbox();
    place_player(0.0f, 0.0f);
    auto* enemy = scene_->build().enemy(0.0f, 5.0f, FACING_MINUS_Z);
    ASSERT_NE(enemy, nullptr);
    step(SETTLE_FRAMES);
    enemy->receive_attack_hit(0, 0.0f);
    step(2);
    ASSERT_EQ(enemy->awareness().state(), Awareness::eAlert);

    throw_smoke_at(position_of(enemy->get_game_object()));
    ASSERT_EQ(clouds().size(), 1u);
    const auto centre = clouds().front()->centre();
    EXPECT_TRUE(enemy->is_blinded());
    EXPECT_EQ(enemy->awareness().state(), Awareness::eSearching);
    EXPECT_LT(glm::distance(glm::vec2(enemy->awareness().last_known_position().x, enemy->awareness().last_known_position().z),
        glm::vec2(centre.x, centre.z)), 0.01f);
    // it doesn't find the player while the cloud lasts
    bool alerted = false;
    step_until(frames(4.0f), [&] { alerted = alerted || enemy->awareness().state() == Awareness::eAlert; return false; });
    EXPECT_FALSE(alerted);
    EXPECT_EQ(player_->get_hp(), player_->get_max_hp());
}

// a cloud between them hides the player from an enemy looking at it; once the cloud is gone it sees the player again
TEST_F(SmokeTest, CloudBetweenHidesThePlayerUntilItFades)
{
    sandbox();
    place_player(0.0f, 0.0f);
    // facing away until the cloud is up, then toward the player (who stays still: silent)
    auto* enemy = scene_->build().enemy(0.0f, 6.6f, FACING_PLUS_Z);
    ASSERT_NE(enemy, nullptr);
    step(SETTLE_FRAMES);
    throw_smoke_at(glm::vec3(0.0f, 0.0f, 3.0f));
    ASSERT_EQ(clouds().size(), 1u);
    EXPECT_FALSE(enemy->is_blinded());
    auto tc = engineGameObjectGetTransformComponent(enemy->get_game_object());
    const auto facing_player = glm::angleAxis(glm::radians(FACING_MINUS_Z), glm::vec3(0.0f, 1.0f, 0.0f));
    tc.rotation[0] = facing_player.x;
    tc.rotation[1] = facing_player.y;
    tc.rotation[2] = facing_player.z;
    tc.rotation[3] = facing_player.w;
    engineGameObjectUpdateTransformComponent(enemy->get_game_object(), &tc);

    bool alerted = false;
    const float life = project_c::default_smoke_tuning().duration_seconds;
    step_until(frames(life - 1.0f), [&] { alerted = alerted || enemy->awareness().state() == Awareness::eAlert; return false; });
    EXPECT_FALSE(alerted);
    // gone: it sees the player
    EXPECT_TRUE(step_until(frames(4.0f), [&] { return enemy->awareness().state() == Awareness::eAlert; }));
    EXPECT_TRUE(clouds().empty());
}

// an enemy blinded by the bomb doesn't attack, and is executed from the front
TEST_F(SmokeTest, BlindedEnemyDoesNotAttackAndIsExecutedFromTheFront)
{
    sandbox();
    place_player(0.0f, 0.0f);
    auto* enemy = scene_->build().enemy(0.0f, 1.2f, FACING_MINUS_Z);
    ASSERT_NE(enemy, nullptr);
    const auto go = enemy->get_game_object();
    step(SETTLE_FRAMES);
    enemy->receive_attack_hit(0, 0.0f);
    step(2);

    throw_smoke_at(position_of(go));
    ASSERT_TRUE(enemy->is_blinded());
    const auto hp = player_->get_hp();
    step(frames(1.0f));
    EXPECT_TRUE(enemy->is_blinded());
    EXPECT_EQ(player_->get_hp(), hp);

    // facing the player, but blinded: an execution
    PlayerInput attack{};
    attack.attack = true;
    step_aiming(1, attack, position_of(go));
    bool executing = false;
    EXPECT_TRUE(step_until(90, [&] { executing = executing || player_->movement_mode() == MovementMode::eExecuting; return is_dead(go); }));
    EXPECT_TRUE(executing);
}

// a blinded pack member ignores its pack: a mate outside the cloud who sees the player doesn't alert it while it is
// blinded (it searches, and stays blinded: it doesn't act); after the blindness its pack alerts it again
TEST_F(SmokeTest, BlindedPackMemberIgnoresAMateWhoSeesThePlayer)
{
    sandbox();
    place_player(0.0f, 0.0f);
    auto config = project_c::default_enemy_spawner();
    config.enemy_type = scenario::app().get_skeleton_warrior_type();
    config.count = 2;
    config.area_half_extents[0] = 1.0f;
    config.area_half_extents[1] = 1.0f;
    config.min_spacing = 0.8f;
    auto* spawner = scene_->build().enemy_spawner(0.0f, 8.0f, config);
    ASSERT_NE(spawner, nullptr);
    step(2);
    std::vector<Enemy*> pack;
    for (const auto go : spawner->get_spawned())
    {
        if (auto* enemy = scene_->get_script<Enemy>(go))
        {
            pack.push_back(enemy);
        }
    }
    ASSERT_EQ(pack.size(), 2u);
    // apart: the blinded one 10 m from its mate, both facing the player
    const auto place = [](Enemy* enemy, float x, float z) {
        auto tc = engineGameObjectGetTransformComponent(enemy->get_game_object());
        tc.position[0] = x;
        tc.position[2] = z;
        const auto facing = glm::angleAxis(glm::atan(-x, -z), glm::vec3(0.0f, 1.0f, 0.0f));
        tc.rotation[0] = facing.x;
        tc.rotation[1] = facing.y;
        tc.rotation[2] = facing.z;
        tc.rotation[3] = facing.w;
        engineGameObjectUpdateTransformComponent(enemy->get_game_object(), &tc);
    };
    auto* blinded = pack[0];
    auto* mate = pack[1];
    place(blinded, -5.0f, 6.0f);
    place(mate, 5.0f, 6.0f);
    step(SETTLE_FRAMES);
    mate->receive_attack_hit(0, 0.0f);
    step(2);
    ASSERT_EQ(mate->awareness().state(), Awareness::eAlert);
    ASSERT_EQ(blinded->awareness().state(), Awareness::eAlert);

    throw_smoke_at(position_of(blinded->get_game_object()));
    ASSERT_TRUE(blinded->is_blinded());
    const float blind = project_c::default_smoke_tuning().blind_seconds;
    bool alerted = false;
    bool stayed_blinded = true;
    step_until(frames(blind - 0.5f), [&] {
        alerted = alerted || blinded->awareness().state() == Awareness::eAlert;
        stayed_blinded = stayed_blinded && blinded->is_blinded();
        return false;
    });
    EXPECT_EQ(mate->awareness().state(), Awareness::eAlert);
    EXPECT_FALSE(alerted);
    EXPECT_TRUE(stayed_blinded);
    EXPECT_EQ(blinded->awareness().state(), Awareness::eSearching);
    // the blindness over, its mate alerts it again
    EXPECT_TRUE(step_until(frames(3.0f), [&] { return !blinded->is_blinded() && blinded->awareness().state() == Awareness::eAlert; }));
}

// an execution the enemy survives (its type's execution_kills is false) ends the blindness like a hit: it is alerted,
// and isn't executed from the front again
TEST_F(SmokeTest, SurvivedExecutionEndsTheBlindness)
{
    sandbox();
    place_player(0.0f, 0.0f);
    auto* enemy = scene_->build().enemy(0.0f, 1.2f, FACING_MINUS_Z);
    ASSERT_NE(enemy, nullptr);
    const auto go = enemy->get_game_object();
    const auto file = std::filesystem::temp_directory_path() / "project_c_scenario_smoke_heavy_enemy.json";
    std::ofstream(file, std::ios::binary) << R"({ "version": 1, "type": "enemy_type", "value": { "max_hp": 100, "execution_kills": false } })";
    const auto heavy = engineDataAssetLoad(file.generic_string().c_str());
    std::filesystem::remove(file);
    ASSERT_NE(heavy, ENGINE_INVALID_OBJECT_HANDLE);
    const auto type = project_c::game_components().enemy_config;
    auto config = project_c::get_game_component(go, type, project_c::default_enemy_config());
    config.type = heavy;
    ASSERT_EQ(engineGameObjectUpdateDataComponent(go, type, &config), ENGINE_RESULT_CODE_OK);
    step(SETTLE_FRAMES);

    throw_smoke_at(position_of(go));
    ASSERT_TRUE(enemy->is_blinded());
    // the throw's recovery
    step(frames(0.5f));
    ASSERT_TRUE(enemy->is_blinded());
    PlayerInput attack{};
    attack.attack = true;
    step_aiming(1, attack, position_of(go));
    bool executing = false;
    // right after the execution's blow, before anything else hits it
    bool blinded_after_blow = true;
    EXPECT_TRUE(step_until(90, [&] {
        executing = executing || player_->movement_mode() == MovementMode::eExecuting;
        if (enemy->hp < 100u)
        {
            blinded_after_blow = enemy->is_blinded();
            return true;
        }
        return false;
    }));
    EXPECT_TRUE(executing);
    EXPECT_FALSE(blinded_after_blow);
    step(2);
    EXPECT_EQ(enemy->awareness().state(), Awareness::eAlert);
    EXPECT_TRUE(step_until(90, [&] { return player_->movement_mode() != MovementMode::eExecuting; }));

    // it has noticed the player, so from the front it is only attacked
    step_aiming(1, attack, position_of(go));
    bool executed_again = false;
    step_until(60, [&] { executed_again = executed_again || player_->movement_mode() == MovementMode::eExecuting; return false; });
    EXPECT_FALSE(executed_again);
    engineDataAssetRelease(heavy);
}

// a hit ends the blindness: the enemy is alerted at once
TEST_F(SmokeTest, HitEndsTheBlindness)
{
    sandbox();
    place_player(-4.0f, 0.0f);
    auto* enemy = scene_->build().enemy(2.0f, 0.0f, FACING_PLUS_X);
    ASSERT_NE(enemy, nullptr);
    step(SETTLE_FRAMES);
    throw_smoke_at(position_of(enemy->get_game_object()));
    ASSERT_TRUE(enemy->is_blinded());

    enemy->receive_attack_hit(0, 0.0f);
    step(2);
    EXPECT_FALSE(enemy->is_blinded());
    EXPECT_EQ(enemy->awareness().state(), Awareness::eAlert);
}

// the cooldown refuses a second throw, and allows one after it
TEST_F(SmokeTest, CooldownRefusesASecondThrowUntilItEnds)
{
    sandbox();
    place_player(0.0f, 0.0f);
    step(SETTLE_FRAMES);
    const auto tuning = project_c::default_smoke_tuning();

    throw_smoke_at(glm::vec3(3.0f, 0.0f, 0.0f));
    ASSERT_EQ(clouds().size(), 1u);
    EXPECT_GT(player_->smoke_cooldown_remaining(), tuning.cooldown_seconds - 1.0f);
    step(frames(0.5f));
    throw_smoke_at(glm::vec3(-3.0f, 0.0f, 0.0f), 30);
    EXPECT_EQ(clouds().size(), 1u);

    // after the cooldown (the first cloud is gone by then)
    step(frames(tuning.cooldown_seconds));
    EXPECT_EQ(player_->smoke_cooldown_remaining(), 0.0f);
    EXPECT_TRUE(clouds().empty());
    throw_smoke_at(glm::vec3(-3.0f, 0.0f, 0.0f));
    EXPECT_EQ(clouds().size(), 1u);
}

// the pop is a loud noise: an unaware enemy within the noise radius becomes suspicious, one beyond it notices nothing
TEST_F(SmokeTest, PopIsHeardWithinTheNoiseRadiusOnly)
{
    sandbox();
    place_player(-8.0f, 0.0f);
    // facing away from the player; 4 m and 7 m from where the bomb lands (outside the cloud's radius)
    auto* near_enemy = scene_->build().enemy(0.0f, 4.0f, FACING_PLUS_Z);
    auto* far_enemy = scene_->build().enemy(0.0f, -7.0f, FACING_MINUS_Z);
    ASSERT_NE(near_enemy, nullptr);
    ASSERT_NE(far_enemy, nullptr);
    step(SETTLE_FRAMES);
    ASSERT_EQ(near_enemy->awareness().state(), Awareness::eUnaware);
    ASSERT_EQ(far_enemy->awareness().state(), Awareness::eUnaware);

    throw_smoke_at(glm::vec3(0.0f, 0.0f, 0.0f));
    step(2);
    EXPECT_FALSE(near_enemy->is_blinded());
    EXPECT_EQ(near_enemy->awareness().state(), Awareness::eSuspicious);
    EXPECT_EQ(far_enemy->awareness().state(), Awareness::eUnaware);
}

// a throw farther than the range lands at the range, in the same direction
TEST_F(SmokeTest, ThrowBeyondRangeLandsAtRange)
{
    sandbox();
    place_player(-6.0f, 0.0f);
    step(SETTLE_FRAMES);
    const auto tuning = project_c::default_smoke_tuning();

    throw_smoke_at(glm::vec3(10.0f, 0.0f, 0.0f));
    ASSERT_EQ(clouds().size(), 1u);
    const auto centre = clouds().front()->centre();
    const auto player = position_of(player_->get_game_object());
    EXPECT_NEAR(glm::distance(glm::vec2(centre.x, centre.z), glm::vec2(player.x, player.z)), tuning.throw_range, 0.1f);
    EXPECT_NEAR(centre.z, 0.0f, 0.1f);
}
