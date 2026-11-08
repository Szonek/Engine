#include "sandbox_scene.h"

#include <app.h>
#include <scripts/building_script.h>
#include <scripts/enemy_script.h>
#include <scripts/enemy_spawner_script.h>
#include <scripts/game_components.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <functional>
#include <vector>

// Enemy awareness: what enemies notice of the player (sight, hearing, damage, their pack) and what they do about it.
// Outcomes (the awareness state an enemy reaches, whether it hits the player) with generous time limits, so tuning
// changes don't break them.
namespace
{
using project_c::Awareness;
using project_c::Enemy;
using project_c::Player;
using project_c::PlayerInput;
using scenario::FIXED_DT_MS;

// frames for the player and the enemies, spawned above the floor, to land
constexpr std::uint32_t SETTLE_FRAMES = 45;
// yaw (around +y) facing -z, +x, -x
constexpr float FACING_MINUS_Z = 180.0f;
constexpr float FACING_MINUS_X = -90.0f;

float seconds(std::uint32_t frames)
{
    return frames * FIXED_DT_MS / 1000.0f;
}

PlayerInput moving_plus_z(bool sneak)
{
    PlayerInput input{};
    // S moves the player toward +z
    input.back = true;
    input.sneak = sneak;
    return input;
}

class AwarenessTest : public scenario::GameplayTest
{
protected:
    scenario::SandboxScene* sandbox(float half_size = 7.0f)
    {
        scene_ = load_sandbox_scene();
        scene_->build().floor_area(-half_size, -half_size, half_size, half_size);
        return scene_;
    }

    // Steps with `input` until `done` (checked after every frame) or `max_frames`; the frames stepped, max_frames + 1
    // when it never got done. `each_frame` sees every frame.
    std::uint32_t step_until(const PlayerInput& input, std::uint32_t max_frames, const std::function<bool()>& done,
        const std::function<void()>& each_frame = {})
    {
        set_input(input);
        for (std::uint32_t frame = 1; frame <= max_frames; frame++)
        {
            step();
            if (each_frame)
            {
                each_frame();
            }
            if (done())
            {
                return frame;
            }
        }
        return max_frames + 1;
    }

    static void face(engine_game_object_t go, float yaw_degrees)
    {
        auto tc = engineGameObjectGetTransformComponent(go);
        const auto rotation = glm::angleAxis(glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
        tc.rotation[0] = rotation.x;
        tc.rotation[1] = rotation.y;
        tc.rotation[2] = rotation.z;
        tc.rotation[3] = rotation.w;
        engineGameObjectUpdateTransformComponent(go, &tc);
    }

    std::vector<Enemy*> members(const project_c::EnemySpawner& spawner)
    {
        std::vector<Enemy*> enemies;
        for (const auto go : spawner.get_spawned())
        {
            if (auto* enemy = scene_->get_script<Enemy>(go))
            {
                enemies.push_back(enemy);
            }
        }
        return enemies;
    }

    static bool all_in(const std::vector<Enemy*>& enemies, Awareness state)
    {
        return std::all_of(enemies.begin(), enemies.end(), [state](const Enemy* enemy) { return enemy->awareness().state() == state; });
    }

    project_c::EnemySpawner* pack(float x, float z, std::uint32_t seed)
    {
        auto config = project_c::default_enemy_spawner();
        config.enemy_type = scenario::app().get_skeleton_warrior_type();
        config.count = 2;
        config.area_half_extents[0] = 1.0f;
        config.area_half_extents[1] = 1.0f;
        config.min_spacing = 0.8f;
        config.seed = seed;
        return scene_->build().enemy_spawner(x, z, config);
    }

    // seconds until an enemy facing -x, 3 m from the player, is alert; the player on the house's roof or on the floor
    float seconds_to_alert(bool on_roof)
    {
        auto* scene = sandbox();
        const float player_x = 2.0f;
        if (on_roof)
        {
            EXPECT_NE(scene->build().house(0.0f, 0.0f, 0.0f), nullptr);
        }
        auto* player = scene->build().player(player_x, 1.0f);
        auto* enemy = scene->build().enemy(player_x + 3.0f, 1.0f, FACING_MINUS_X);
        EXPECT_NE(enemy, nullptr);
        if (on_roof)
        {
            auto tc = engineGameObjectGetTransformComponent(player->get_game_object());
            tc.position[1] = project_c::House::wall_height + project_c::House::roof_thickness + 0.05f;
            engineGameObjectUpdateTransformComponent(player->get_game_object(), &tc);
        }
        scene->snap_camera_to_player();
        const auto frames = step_until({}, 600, [&] { return enemy->awareness().state() == Awareness::eAlert; });
        TearDown();
        return seconds(frames);
    }

    scenario::SandboxScene* scene_ = nullptr;
};
}  // namespace

// sneaking up behind an enemy to 1 m: it notices nothing
TEST_F(AwarenessTest, EnemyFacingAwayDoesNotNoticeASneakingPlayer)
{
    auto* scene = sandbox();
    // the enemy faces +z, the player comes from behind it (-z)
    auto* enemy = scene->build().enemy(0.0f, 0.0f);
    auto* player = scene->build().player(0.0f, -4.0f);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);

    bool noticed = false;
    const auto reached = step_until(moving_plus_z(true), 400, [&] { return position_of(player->get_game_object()).z >= -1.0f; },
        [&] { noticed = noticed || enemy->awareness().state() != Awareness::eUnaware; });
    ASSERT_LE(reached, 400u);
    // stopping while still sneaking (letting go of Shift while slowing down would be heard)
    PlayerInput still{};
    still.sneak = true;
    step_until(still, 60, [] { return false; }, [&] { noticed = noticed || enemy->awareness().state() != Awareness::eUnaware; });
    EXPECT_FALSE(noticed);
}

// the same path at a run: it hears the footsteps, turns to them and sees the player
TEST_F(AwarenessTest, EnemyFacingAwayHearsARunningPlayer)
{
    auto* scene = sandbox();
    auto* enemy = scene->build().enemy(0.0f, 0.0f);
    auto* player = scene->build().player(0.0f, -4.0f);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);

    bool suspicious = false;
    step_until(moving_plus_z(false), 400, [&] { return position_of(player->get_game_object()).z >= -1.0f; },
        [&] { suspicious = suspicious || enemy->awareness().state() == Awareness::eSuspicious; });
    EXPECT_TRUE(suspicious);
    const auto alert = step_until({}, 240, [&] { return enemy->awareness().state() == Awareness::eAlert; },
        [&] { suspicious = suspicious || enemy->awareness().state() == Awareness::eSuspicious; });
    EXPECT_LE(alert, 240u);
}

TEST_F(AwarenessTest, EnemyInFrontSeesThePlayerAndAttacks)
{
    auto* scene = sandbox();
    auto* player = scene->build().player(0.0f, 0.0f);
    auto* enemy = scene->build().enemy(0.0f, 3.0f, FACING_MINUS_Z);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();

    const auto alert = step_until({}, 180, [&] { return enemy->awareness().state() == Awareness::eAlert; });
    EXPECT_LE(alert, 180u);
    const auto hit = step_until({}, 300, [&] { return player->get_hp() < player->get_max_hp(); });
    EXPECT_LE(hit, 300u);
}

// a wall between them: the enemy looking at the player doesn't see it (and a player standing still makes no noise)
TEST_F(AwarenessTest, WallBlocksSight)
{
    auto* scene = sandbox();
    scene->build().obstacle(glm::vec3(0.0f, 1.0f, 1.5f), glm::vec3(3.0f, 2.0f, 0.3f));
    scene->build().player(0.0f, 0.0f);
    auto* enemy = scene->build().enemy(0.0f, 3.0f, FACING_MINUS_Z);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();

    bool noticed = false;
    step_until({}, 240, [] { return false; }, [&] { noticed = noticed || enemy->awareness().state() != Awareness::eUnaware; });
    EXPECT_FALSE(noticed);
}

TEST_F(AwarenessTest, PlayerOnARoofIsSeenMoreSlowly)
{
    const float on_floor = seconds_to_alert(false);
    const float on_roof = seconds_to_alert(true);
    EXPECT_LT(on_floor, 2.0f);
    EXPECT_LT(on_roof, 8.0f);
    // above_sight_multiplier 0.35: about three times as long
    EXPECT_GT(on_roof, on_floor * 2.0f);
}

// one enemy of a pack sees the player: its pack is alerted with it, the enemies of another spawner aren't
TEST_F(AwarenessTest, AlertingOneEnemyAlertsItsPack)
{
    auto* scene = sandbox(10.0f);
    auto* player = scene->build().player(0.0f, -6.0f);
    auto* near_pack = pack(-6.0f, 0.0f, 1);
    auto* far_pack = pack(7.0f, 6.0f, 2);
    scene->snap_camera_to_player();
    step(2);
    const auto pack_members = members(*near_pack);
    const auto others = members(*far_pack);
    ASSERT_EQ(pack_members.size(), 2u);
    ASSERT_EQ(others.size(), 2u);
    step(SETTLE_FRAMES);
    ASSERT_TRUE(all_in(pack_members, Awareness::eUnaware));

    // the player walks in front of one of them, 3 m away, the other one faces away from it
    const auto watcher = pack_members[0]->get_game_object();
    const auto watcher_position = position_of(watcher);
    face(watcher, FACING_MINUS_Z);
    face(pack_members[1]->get_game_object(), 0.0f);
    auto tc = engineGameObjectGetTransformComponent(player->get_game_object());
    tc.position[0] = watcher_position.x;
    tc.position[2] = watcher_position.z - 3.0f;
    engineGameObjectUpdateTransformComponent(player->get_game_object(), &tc);

    const auto alert = step_until({}, 240, [&] { return all_in(pack_members, Awareness::eAlert); });
    EXPECT_LE(alert, 240u);
    EXPECT_TRUE(all_in(others, Awareness::eUnaware));
}

// once no member perceives the player any more, the pack searches, gives up and forgets
TEST_F(AwarenessTest, PackCalmsDownAfterLosingThePlayer)
{
    auto* scene = sandbox(10.0f);
    auto* player = scene->build().player(0.0f, -6.0f);
    auto* spawner = pack(-6.0f, 0.0f, 1);
    scene->snap_camera_to_player();
    step(2);
    const auto pack_members = members(*spawner);
    ASSERT_EQ(pack_members.size(), 2u);
    step(SETTLE_FRAMES);

    pack_members[0]->receive_attack_hit(0, 0.0f);
    const auto alert = step_until({}, 30, [&] { return all_in(pack_members, Awareness::eAlert); });
    ASSERT_LE(alert, 30u);

    // the player is gone: nothing is perceived any more
    engineGameObjectDestroy(player->get_game_object());
    // both search where the player was, give up and forget: the grace time, lose_seconds and the meter's decay, with
    // a margin
    bool searched = false;
    const auto calm = step_until({}, 60 * 15, [&] { return all_in(pack_members, Awareness::eUnaware); },
        [&] { searched = searched || all_in(pack_members, Awareness::eSearching); });
    EXPECT_TRUE(searched);
    EXPECT_LE(calm, 60u * 15u);
}

// a pack of one forgets the player as fast as a placed enemy (regression: its own perception kept it alert through
// its pack for lose_seconds)
TEST_F(AwarenessTest, PackMemberForgetsLikeAPlacedEnemy)
{
    auto* scene = sandbox(10.0f);
    auto* player = scene->build().player(0.0f, -6.0f);
    auto* placed = scene->build().enemy(6.0f, 0.0f);
    auto config = project_c::default_enemy_spawner();
    config.enemy_type = scenario::app().get_skeleton_warrior_type();
    config.count = 1;
    config.area_half_extents[0] = 0.5f;
    config.area_half_extents[1] = 0.5f;
    auto* spawner = scene->build().enemy_spawner(-6.0f, 0.0f, config);
    ASSERT_NE(placed, nullptr);
    scene->snap_camera_to_player();
    step(2);
    const auto pack_members = members(*spawner);
    ASSERT_EQ(pack_members.size(), 1u);
    auto* spawned = pack_members[0];
    step(SETTLE_FRAMES);

    placed->receive_attack_hit(0, 0.0f);
    spawned->receive_attack_hit(0, 0.0f);
    step(1);
    ASSERT_EQ(placed->awareness().state(), Awareness::eAlert);
    ASSERT_EQ(spawned->awareness().state(), Awareness::eAlert);
    engineGameObjectDestroy(player->get_game_object());

    std::uint32_t placed_searching = 0;
    std::uint32_t spawned_searching = 0;
    for (std::uint32_t frame = 1; frame <= 600 && (placed_searching == 0 || spawned_searching == 0); frame++)
    {
        step();
        if (placed_searching == 0 && placed->awareness().state() == Awareness::eSearching)
        {
            placed_searching = frame;
        }
        if (spawned_searching == 0 && spawned->awareness().state() == Awareness::eSearching)
        {
            spawned_searching = frame;
        }
    }
    ASSERT_GT(placed_searching, 0u);
    ASSERT_GT(spawned_searching, 0u);
    EXPECT_NEAR(static_cast<float>(spawned_searching), static_cast<float>(placed_searching), 2.0f);
}

// a hit on an unaware enemy alerts it and its pack at once, with the player's position as the last known one
TEST_F(AwarenessTest, HitWhileUnawareAlertsThePack)
{
    auto* scene = sandbox(10.0f);
    auto* player = scene->build().player(0.0f, -6.0f);
    auto* spawner = pack(-6.0f, 0.0f, 1);
    scene->snap_camera_to_player();
    step(2);
    const auto pack_members = members(*spawner);
    ASSERT_EQ(pack_members.size(), 2u);
    step(SETTLE_FRAMES);
    ASSERT_TRUE(all_in(pack_members, Awareness::eUnaware));

    pack_members[0]->receive_attack_hit(project_c::default_player_tuning().attack_damage, 0.0f);
    step(1);
    EXPECT_EQ(pack_members[0]->awareness().state(), Awareness::eAlert);
    EXPECT_FLOAT_EQ(pack_members[0]->awareness().meter(), 1.0f);
    const auto player_position = position_of(player->get_game_object());
    EXPECT_NEAR(pack_members[0]->awareness().last_known_position().x, player_position.x, 0.1f);
    EXPECT_NEAR(pack_members[0]->awareness().last_known_position().z, player_position.z, 0.1f);
    // the rest of the pack a frame later at most (its state is computed from the members each frame)
    step(2);
    EXPECT_TRUE(all_in(pack_members, Awareness::eAlert));
}

// the alert doesn't wait for the hit stop to end (regression: the enemy stayed unaware for the whole hit stop)
TEST_F(AwarenessTest, HitAlertsDuringTheHitStop)
{
    auto* scene = sandbox();
    scene->build().player(0.0f, -6.0f);
    auto* enemy = scene->build().enemy(0.0f, 0.0f);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);
    ASSERT_EQ(enemy->awareness().state(), Awareness::eUnaware);

    enemy->receive_attack_hit(0, 0.5f);
    step(1);
    EXPECT_EQ(enemy->awareness().state(), Awareness::eAlert);
}

// a killing blow alerts the pack too, and the corpse doesn't keep it alert (regression: a one-hit kill alerted nobody,
// and an enemy killed while alert held its pack alert during its death)
TEST_F(AwarenessTest, KillingBlowAlertsThePackButTheCorpseDoesNotHoldIt)
{
    auto* scene = sandbox(10.0f);
    auto* player = scene->build().player(0.0f, -6.0f);
    auto* spawner = pack(-6.0f, 0.0f, 1);
    scene->snap_camera_to_player();
    step(2);
    const auto pack_members = members(*spawner);
    ASSERT_EQ(pack_members.size(), 2u);
    step(SETTLE_FRAMES);
    ASSERT_TRUE(all_in(pack_members, Awareness::eUnaware));

    auto* survivor = pack_members[1];
    pack_members[0]->receive_attack_hit(pack_members[0]->get_max_hp(), 0.0f);
    const auto alert = step_until({}, 10, [&] { return survivor->awareness().state() == Awareness::eAlert; });
    EXPECT_LE(alert, 10u);

    // nothing perceives the player any more: the survivor searches after its grace time, while the corpse's death
    // still plays
    engineGameObjectDestroy(player->get_game_object());
    const auto searching = step_until({}, 120, [&] { return survivor->awareness().state() == Awareness::eSearching; });
    EXPECT_LE(seconds(searching), project_c::default_enemy_perception().lose_sight_grace_seconds + 0.5f);
}

// another enemy between them doesn't hide the player, only level geometry does (regression: the rear one of a file of
// enemies never saw the player)
TEST_F(AwarenessTest, OtherEnemiesDoNotBlockSight)
{
    auto* scene = sandbox();
    scene->build().player(0.0f, 0.0f);
    // placed enemies: packs of one, so the rear one can only see the player itself
    auto* front = scene->build().enemy(0.0f, 1.5f, FACING_MINUS_Z);
    auto* rear = scene->build().enemy(0.0f, 3.0f, FACING_MINUS_Z);
    ASSERT_NE(front, nullptr);
    ASSERT_NE(rear, nullptr);
    scene->snap_camera_to_player();

    const auto alert = step_until({}, 180, [&] { return rear->awareness().state() == Awareness::eAlert; });
    EXPECT_LE(alert, 180u);
}

// a file of enemies between them doesn't hide the player either, however many (sight is one ray against the level
// geometry's collision layer; it used to look past at most four characters)
TEST_F(AwarenessTest, EnemyBehindAFileOfOthersSeesThePlayer)
{
    auto* scene = sandbox();
    scene->build().player(0.0f, 0.0f);
    for (int i = 0; i < 5; i++)
    {
        ASSERT_NE(scene->build().enemy(0.0f, 1.0f + 0.8f * static_cast<float>(i), FACING_MINUS_Z), nullptr);
    }
    auto* rear = scene->build().enemy(0.0f, 5.0f, FACING_MINUS_Z);
    ASSERT_NE(rear, nullptr);
    scene->snap_camera_to_player();

    const auto alert = step_until({}, 240, [&] { return rear->awareness().state() == Awareness::eAlert; });
    EXPECT_LE(alert, 240u);
}
