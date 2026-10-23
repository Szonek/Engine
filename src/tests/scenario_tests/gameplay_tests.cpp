#include "sandbox_scene.h"

#include <scripts/encounter_script.h>
#include <scripts/enemy_script.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

// Gameplay scenarios on tiny sandbox scenes. They check outcomes (the enemy died, the encounter is clear, the player
// didn't pass through the wall) with tolerances, not exact values, so tuning changes don't break them.
namespace
{
using project_c::Encounter;
using project_c::PlayerInput;
using scenario::FIXED_DT_MS;

// frames for the player, spawned 1 m above the floor, to land
constexpr std::uint32_t LANDING_FRAMES = 30;
// where scenarios spawn the player
constexpr float PLAYER_X = 1.0f;
constexpr float PLAYER_Z = 1.0f;

float seconds(std::uint32_t frames)
{
    return frames * FIXED_DT_MS / 1000.0f;
}

float horizontal_distance(const glm::vec3& a, const glm::vec3& b)
{
    return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
}

class MovementTest : public scenario::GameplayTest
{
protected:
    // average horizontal speed (m/s) while holding `input`, measured after the player has accelerated
    float measure_speed(const PlayerInput& input)
    {
        auto* scene = load_sandbox_scene();
        scene->build().floor_area(-6.0f, -6.0f, 6.0f, 6.0f);
        auto* player = scene->build().player(PLAYER_X, PLAYER_Z);
        scene->snap_camera_to_player();
        step(LANDING_FRAMES);

        set_input(input);
        step(15);
        const auto start = position_of(player->get_game_object());
        constexpr std::uint32_t measured_frames = 30;
        step(measured_frames);
        const auto end = position_of(player->get_game_object());
        release_all_input();
        return horizontal_distance(start, end) / seconds(measured_frames);
    }
};

// regression: diagonal movement used to be faster than straight movement
TEST_F(MovementTest, DiagonalSpeedEqualsStraightSpeed)
{
    PlayerInput straight{};
    straight.forward = true;
    const auto straight_speed = measure_speed(straight);
    TearDown();

    PlayerInput diagonal{};
    diagonal.forward = true;
    diagonal.right = true;
    const auto diagonal_speed = measure_speed(diagonal);

    EXPECT_GT(straight_speed, 1.0f);
    EXPECT_NEAR(diagonal_speed, straight_speed, straight_speed * 0.05f);
}

// A new game object's colliders used to be synced to physics before its first world transform was computed: children
// (the player's attack trigger) got an uninitialized transform for that frame, which made Bullet assert on
// rays near the world origin, where the debug reset key puts the player.
TEST_F(MovementTest, PlayerAtWorldOriginLands)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-3.0f, -3.0f, 3.0f, 3.0f);
    auto* player = scene->build().player(0.0f, 0.0f);
    scene->snap_camera_to_player();
    step(LANDING_FRAMES);
    EXPECT_NEAR(position_of(player->get_game_object()).x, 0.0f, 0.05f);

    // walk away, then the debug reset key puts the player back at the origin
    PlayerInput walk{};
    walk.right = true;
    set_input(walk);
    step(30);
    PlayerInput reset{};
    reset.debug_reset_position = true;
    set_input(reset);
    step();
    release_all_input();
    step(LANDING_FRAMES);

    // near the origin: it brakes from walking speed after the reset
    const auto position = position_of(player->get_game_object());
    EXPECT_NEAR(position.x, 0.0f, 0.25f);
    EXPECT_NEAR(position.z, 0.0f, 0.25f);
    EXPECT_FALSE(player->is_dead());
}

// the player used to bounce off colliders instead of sliding along them (a dynamic body moved by its transform)
TEST_F(MovementTest, WalkingDiagonallyIntoWallSlidesAlongIt)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-6.0f, -6.0f, 6.0f, 6.0f);
    // wall along z, its surface facing the player 0.9 m to its right
    const auto wall_surface_x = PLAYER_X + 0.9f;
    scene->build().obstacle({ wall_surface_x + 0.1f, 1.0f, PLAYER_Z }, { 0.2f, 2.0f, 8.0f });
    auto* player = scene->build().player(PLAYER_X, PLAYER_Z);
    scene->snap_camera_to_player();
    step(LANDING_FRAMES);

    PlayerInput into_wall{};
    into_wall.right = true;
    into_wall.forward = true;
    set_input(into_wall);
    const auto start = position_of(player->get_game_object());
    float max_x = std::numeric_limits<float>::lowest();
    float min_x_after_contact = std::numeric_limits<float>::max();
    for (int i = 0; i < 90; i++)
    {
        step();
        const auto x = position_of(player->get_game_object()).x;
        if (x >= max_x)
        {
            max_x = x;
            min_x_after_contact = x;
        }
        min_x_after_contact = std::min(min_x_after_contact, x);
    }
    const auto end = position_of(player->get_game_object());

    // never inside the wall: the player's capsule has a radius of 0.15 m, so its center stays at least 0.1 m from the
    // wall surface
    EXPECT_LT(max_x, wall_surface_x - 0.1f);
    // no bounce: it stays against the wall once it got there
    EXPECT_LT(max_x - min_x_after_contact, 0.1f);
    // slides along the wall (-z) instead of stopping
    EXPECT_GT(start.z - end.z, 1.0f);
}

class CombatTest : public scenario::GameplayTest
{
protected:
    // presses attack (one frame down, then released) and lets the swing play while aiming at `target`
    void attack(const glm::vec3& target, std::uint32_t frames_after_press)
    {
        PlayerInput press{};
        press.attack = true;
        step_aiming(1, press, target);
        step_aiming(frames_after_press, {}, target);
    }
};

TEST_F(CombatTest, AttackComboKillsEnemyAndClearsEncounter)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-3.0f, -3.0f, 4.0f, 4.0f);
    scene->build().player(PLAYER_X, PLAYER_Z);
    auto* enemy = scene->build().enemy(PLAYER_X, PLAYER_Z + 0.5f);
    ASSERT_NE(enemy, nullptr);
    const auto enemy_go = enemy->get_game_object();
    scene->snap_camera_to_player();
    step(LANDING_FRAMES);
    EXPECT_EQ(scene->encounter()->get_state(), Encounter::State::eInProgress);

    // the 3-hit combo, one press per swing; a 20 hp enemy needs two of the 10 damage hits
    const auto target = position_of(enemy_go) + glm::vec3(0.0f, 0.35f, 0.0f);
    for (int press = 0; press < 3 && enemy->hp > 0; press++)
    {
        attack(target, 20);
    }
    EXPECT_EQ(enemy->hp, 0u);

    step(2);
    EXPECT_EQ(scene->encounter()->get_state(), Encounter::State::eCleared);
    EXPECT_EQ(scene->encounter()->get_remaining_enemies(), 0u);
}

// enemies close in until their blade reaches the player, from right next to it up to the end of their nav path
// range; they used to stop and swing from up to 1.15 m although the blade only hits within ~0.5 m
TEST_F(CombatTest, EnemyNearbyDamagesIdlePlayer)
{
    for (const float distance : { 0.4f, 0.8f, 2.0f })
    {
        SCOPED_TRACE(distance);
        auto* scene = load_sandbox_scene();
        scene->build().floor_area(-3.0f, -3.0f, 4.0f, 4.0f);
        auto* player = scene->build().player(PLAYER_X, PLAYER_Z);
        scene->build().enemy(PLAYER_X, PLAYER_Z + distance);
        scene->snap_camera_to_player();

        step(LANDING_FRAMES + 240);

        EXPECT_LT(player->get_hp(), player->get_max_hp());
        EXPECT_FALSE(player->is_dead());
        EXPECT_EQ(scene->encounter()->get_state(), Encounter::State::eInProgress);
        TearDown();
    }
}

// the nav mesh is built from the static colliders: the enemy walks around an obstacle between it and the player
TEST_F(CombatTest, EnemyWalksAroundObstacleToPlayer)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-3.0f, -3.0f, 4.0f, 4.0f);
    auto* player = scene->build().player(PLAYER_X, PLAYER_Z);
    scene->build().obstacle({ PLAYER_X, 0.625f, PLAYER_Z + 1.0f }, { 0.8f, 1.0f, 0.3f });
    auto* enemy = scene->build().enemy(PLAYER_X, PLAYER_Z + 2.0f);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();

    step(LANDING_FRAMES + 300);

    EXPECT_LT(player->get_hp(), player->get_max_hp());
    EXPECT_FALSE(player->is_dead());
}

// getting hit puts the player into a short hit-stop; an attack pressed during it used to be dropped
TEST_F(CombatTest, AttackPressedDuringHitStopIsNotLost)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-3.0f, -3.0f, 4.0f, 4.0f);
    auto* player = scene->build().player(PLAYER_X, PLAYER_Z);
    auto* enemy = scene->build().enemy(PLAYER_X, PLAYER_Z + 0.5f);
    ASSERT_NE(enemy, nullptr);
    const auto target = position_of(enemy->get_game_object()) + glm::vec3(0.0f, 0.35f, 0.0f);
    scene->snap_camera_to_player();
    step(LANDING_FRAMES);
    ASSERT_EQ(enemy->hp, enemy->get_max_hp());

    player->receive_damage(1, position_of(enemy->get_game_object()));
    attack(target, 40);

    EXPECT_LT(enemy->hp, enemy->get_max_hp());
}

TEST_F(CombatTest, PlayerDeathDefeatsEncounter)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-3.0f, -3.0f, 4.0f, 4.0f);
    auto* player = scene->build().player(PLAYER_X, PLAYER_Z);
    // out of reach, so only the test hurts the player
    scene->build().enemy(-2.5f, -2.5f);
    scene->snap_camera_to_player();
    step(LANDING_FRAMES);

    player->receive_damage(player->get_max_hp(), position_of(player->get_game_object()) + glm::vec3(0.0f, 0.0f, 1.0f));
    step(2);

    EXPECT_TRUE(player->is_dead());
    EXPECT_EQ(scene->encounter()->get_state(), Encounter::State::eDefeated);
}

TEST_F(CombatTest, EncounterWithoutEnemiesIsNeverCleared)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-2.0f, -2.0f, 2.0f, 2.0f);
    scene->build().player(PLAYER_X, PLAYER_Z);
    scene->snap_camera_to_player();
    step(LANDING_FRAMES);

    EXPECT_EQ(scene->encounter()->get_state(), Encounter::State::eInProgress);
}
}  // namespace
