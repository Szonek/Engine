#include "sandbox_scene.h"

#include <app.h>
#include <scripts/enemy_script.h>
#include <scripts/game_components.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

// Falls, shoves and momentum (FALLS_AND_SHOVES.md): enemies hurt and killed by falls, the kick which sends them off
// ledges, the player's own fall damage and roll, and the momentum meter. Outcomes with generous time limits.
namespace
{
using project_c::Awareness;
using project_c::Enemy;
using project_c::MovementMode;
using project_c::Player;
using project_c::PlayerInput;

// frames for the player and the enemies, spawned above the floor, to land
constexpr std::uint32_t SETTLE_FRAMES = 45;
// yaw (around +y): facing +z, -z, +x, -x
constexpr float FACING_PLUS_Z = 0.0f;
constexpr float FACING_MINUS_Z = 180.0f;
constexpr float FACING_PLUS_X = 90.0f;
constexpr float FACING_MINUS_X = -90.0f;
// the house roof's height above the floor, and a lower ledge
constexpr float ROOF_HEIGHT = 2.25f;
constexpr float LOW_LEDGE_HEIGHT = 1.5f;

class FallTest : public scenario::GameplayTest
{
protected:
    scenario::SandboxScene* sandbox(float half_size = 8.0f)
    {
        scene_ = load_sandbox_scene();
        scene_->build().floor_area(-half_size, -half_size, half_size, half_size);
        return scene_;
    }

    static void set_height(engine_game_object_t go, float y)
    {
        auto tc = engineGameObjectGetTransformComponent(go);
        tc.position[1] = y;
        engineGameObjectUpdateTransformComponent(go, &tc);
    }

    // steps until `done` (checked after every frame); false when it isn't done within `max_frames`
    bool step_until(const PlayerInput& input, std::uint32_t max_frames, const std::function<bool()>& done)
    {
        set_input(input);
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

    // dead, dying or gone
    bool is_dead(engine_game_object_t enemy) const
    {
        const auto* script = scene_->get_script<Enemy>(enemy);
        return !script || script->is_dying();
    }

    scenario::SandboxScene* scene_ = nullptr;
};
}  // namespace

// an enemy dropped from the house roof's height dies when it lands
TEST_F(FallTest, EnemyFallingFromRoofHeightDies)
{
    auto* scene = sandbox();
    // far away and still: the enemy neither sees nor hears the player
    scene->build().player(-6.0f, -6.0f);
    auto* enemy = scene->build().enemy(3.0f, 0.0f, FACING_PLUS_X);
    ASSERT_NE(enemy, nullptr);
    const auto go = enemy->get_game_object();
    set_height(go, ROOF_HEIGHT);
    scene->snap_camera_to_player();

    bool fell = false;
    EXPECT_TRUE(step_until({}, 120, [&] { fell = fell || (!is_dead(go) && enemy->is_falling()); return is_dead(go); }));
    EXPECT_TRUE(fell);
}

// from a lower ledge it survives, hurt, lies a moment and stands up alerted
TEST_F(FallTest, EnemyFallingFromALowLedgeIsHurtAndKnockedDown)
{
    auto* scene = sandbox();
    scene->build().player(-6.0f, -6.0f);
    auto* enemy = scene->build().enemy(3.0f, 0.0f, FACING_PLUS_X);
    ASSERT_NE(enemy, nullptr);
    const auto go = enemy->get_game_object();
    set_height(go, LOW_LEDGE_HEIGHT);
    scene->snap_camera_to_player();

    ASSERT_TRUE(step_until({}, 120, [&] { return enemy->is_knocked_down(); }));
    EXPECT_FALSE(is_dead(go));
    EXPECT_LT(enemy->hp, enemy->get_max_hp());
    // half way between the safe and the lethal height: about half its HP
    EXPECT_NEAR(static_cast<float>(enemy->hp), enemy->get_max_hp() * 0.5f, enemy->get_max_hp() * 0.15f);
    // alerted (it may search once it doesn't find the player)
    bool alerted = false;
    ASSERT_TRUE(step_until({}, 240, [&] { alerted = alerted || enemy->awareness().state() == Awareness::eAlert; return !enemy->is_knocked_down(); }));
    EXPECT_TRUE(alerted);
}

// an enemy spawned just above the floor isn't hurt nor knocked down by its first landing
TEST_F(FallTest, SmallDropDoesNothing)
{
    auto* scene = sandbox();
    scene->build().player(-6.0f, -6.0f);
    auto* enemy = scene->build().enemy(3.0f, 0.0f, FACING_PLUS_X);
    ASSERT_NE(enemy, nullptr);
    set_height(enemy->get_game_object(), 0.5f);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);

    EXPECT_EQ(enemy->hp, enemy->get_max_hp());
    EXPECT_FALSE(enemy->is_knocked_down());
    EXPECT_EQ(enemy->awareness().state(), Awareness::eUnaware);
}

// a body hitting the ground is loud: an unaware enemy within the loud radius becomes suspicious
TEST_F(FallTest, FallDeathIsALoudNoise)
{
    auto* scene = sandbox();
    scene->build().player(-6.0f, -6.0f);
    auto* falling = scene->build().enemy(3.0f, 0.0f, FACING_PLUS_X);
    // 3 m away, facing away from the landing
    auto* listener = scene->build().enemy(3.0f, 3.0f, FACING_PLUS_Z);
    ASSERT_NE(falling, nullptr);
    ASSERT_NE(listener, nullptr);
    const auto falling_go = falling->get_game_object();
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);
    ASSERT_EQ(listener->awareness().state(), Awareness::eUnaware);
    set_height(falling_go, ROOF_HEIGHT + 0.05f);

    bool noticed = false;
    step_until({}, 120, [&] { noticed = noticed || listener->awareness().state() != Awareness::eUnaware; return false; });
    EXPECT_TRUE(is_dead(falling_go));
    EXPECT_TRUE(noticed);
}

namespace
{
class ShoveTest : public FallTest
{
protected:
    // the player at (x, height, z) facing +x, and an enemy 1 m in front of it with its back to it; a ledge (a block
    // from x = -2 to `edge_x`, `height` tall) under both when height > 0
    Enemy* kick_setup(float height, float edge_x = 2.0f, float player_x = 0.6f)
    {
        auto* scene = sandbox();
        if (height > 0.0f)
        {
            scene->build().obstacle(glm::vec3((edge_x - 2.0f) * 0.5f, height * 0.5f, 0.0f), glm::vec3(edge_x + 2.0f, height, 4.0f));
        }
        player_ = scene->build().player(player_x, 0.0f);
        auto* enemy = scene->build().enemy(player_x + 1.0f, 0.0f, FACING_PLUS_X);
        place(player_->get_game_object(), player_x, height, FACING_PLUS_X);
        place(enemy->get_game_object(), player_x + 1.0f, height, FACING_PLUS_X);
        scene->snap_camera_to_player();
        // the player faces the cursor: on the enemy
        aim_ = position(enemy->get_game_object()) + glm::vec3(0.0f, 0.35f, 0.0f);
        step_aiming(SETTLE_FRAMES, {}, aim_);
        return enemy;
    }

    static void place(engine_game_object_t go, float x, float y, float yaw_degrees)
    {
        auto tc = engineGameObjectGetTransformComponent(go);
        tc.position[0] = x;
        tc.position[1] = y + 0.05f;
        const auto rotation = glm::angleAxis(glm::radians(yaw_degrees), glm::vec3(0.0f, 1.0f, 0.0f));
        tc.rotation[0] = rotation.x;
        tc.rotation[1] = rotation.y;
        tc.rotation[2] = rotation.z;
        tc.rotation[3] = rotation.w;
        engineGameObjectUpdateTransformComponent(go, &tc);
    }

    // Q, with the cursor where it was aimed
    void press_shove()
    {
        PlayerInput shove{};
        shove.shove = true;
        step_aiming(1, shove, aim_);
        set_input({});
    }

    static glm::vec3 position(engine_game_object_t go)
    {
        const auto tc = engineGameObjectGetTransformComponent(go);
        return glm::vec3(tc.position[0], tc.position[1], tc.position[2]);
    }

    Player* player_ = nullptr;
    glm::vec3 aim_{ 0.0f };
};
}  // namespace

// a kick on flat ground pushes the enemy well back and staggers it
TEST_F(ShoveTest, KickPushesAnEnemyAndStaggersIt)
{
    auto* enemy = kick_setup(0.0f);
    ASSERT_NE(enemy, nullptr);
    const auto go = enemy->get_game_object();
    const auto start = position(go);

    press_shove();
    EXPECT_TRUE(player_->is_shoving());
    bool staggered = false;
    float pushed = 0.0f;
    step_until({}, 90, [&]
        {
            staggered = staggered || enemy->is_staggered();
            pushed = std::max(pushed, glm::distance(glm::vec2(position(go).x, position(go).z), glm::vec2(start.x, start.z)));
            return false;
        });
    EXPECT_TRUE(staggered);
    EXPECT_GE(pushed, 1.5f);
    EXPECT_FALSE(is_dead(go));
}

// with nobody in front the kick still plays, and pushes nobody (the enemy is behind the player)
TEST_F(ShoveTest, KickWithNobodyInFrontPushesNothing)
{
    auto* scene = sandbox();
    player_ = scene->build().player(0.0f, 0.0f);
    auto* enemy = scene->build().enemy(-1.0f, 0.0f, FACING_MINUS_X);
    ASSERT_NE(enemy, nullptr);
    place(player_->get_game_object(), 0.0f, 0.0f, FACING_PLUS_X);
    scene->snap_camera_to_player();
    // facing away from the enemy
    aim_ = glm::vec3(3.0f, 0.35f, 0.0f);
    step_aiming(SETTLE_FRAMES, {}, aim_);
    press_shove();
    EXPECT_TRUE(player_->is_shoving());
    bool staggered = false;
    step_until({}, 60, [&] { staggered = staggered || enemy->is_staggered(); return false; });
    // not pushed (it may walk to the kick's noise)
    EXPECT_FALSE(staggered);
    EXPECT_EQ(enemy->hp, enemy->get_max_hp());
}

// kicked off the house roof's height it dies when it lands
TEST_F(ShoveTest, EnemyKickedOffARoofDies)
{
    auto* enemy = kick_setup(ROOF_HEIGHT);
    ASSERT_NE(enemy, nullptr);
    const auto go = enemy->get_game_object();

    press_shove();
    bool fell = false;
    EXPECT_TRUE(step_until({}, 180, [&] { fell = fell || (!is_dead(go) && enemy->is_falling()); return is_dead(go); }));
    EXPECT_TRUE(fell);
}

// kicked off a lower ledge it survives, hurt, and is knocked down
TEST_F(ShoveTest, EnemyKickedOffALowLedgeIsHurtAndKnockedDown)
{
    auto* enemy = kick_setup(LOW_LEDGE_HEIGHT);
    ASSERT_NE(enemy, nullptr);
    const auto go = enemy->get_game_object();

    press_shove();
    ASSERT_TRUE(step_until({}, 180, [&] { return enemy->is_knocked_down(); }));
    EXPECT_FALSE(is_dead(go));
    EXPECT_LT(enemy->hp, enemy->get_max_hp());
}

// an enemy which hadn't noticed the player knows about it once it recovers from the kick
TEST_F(ShoveTest, UnawareEnemyKickedBecomesAlert)
{
    auto* enemy = kick_setup(0.0f);
    ASSERT_NE(enemy, nullptr);
    ASSERT_EQ(enemy->awareness().state(), Awareness::eUnaware);

    press_shove();
    bool alerted = false;
    step_until({}, 120, [&] { alerted = alerted || enemy->awareness().state() == Awareness::eAlert; return false; });
    EXPECT_TRUE(alerted);
}

// a wall right behind the enemy stops the push
TEST_F(ShoveTest, KickedEnemyIsStoppedByAWall)
{
    auto* scene = sandbox();
    // a wall 0.5 m behind the enemy
    scene->build().obstacle(glm::vec3(2.3f, 0.75f, 0.0f), glm::vec3(0.2f, 1.5f, 4.0f));
    player_ = scene->build().player(0.6f, 0.0f);
    auto* enemy = scene->build().enemy(1.6f, 0.0f, FACING_PLUS_X);
    ASSERT_NE(enemy, nullptr);
    place(player_->get_game_object(), 0.6f, 0.0f, FACING_PLUS_X);
    scene->snap_camera_to_player();
    const auto go = enemy->get_game_object();
    aim_ = position(go) + glm::vec3(0.0f, 0.35f, 0.0f);
    step_aiming(SETTLE_FRAMES, {}, aim_);

    press_shove();
    float furthest = position(go).x;
    step_until({}, 60, [&] { furthest = std::max(furthest, position(go).x); return false; });
    // the wall's face is at x = 2.2: the capsule stops in front of it
    EXPECT_LT(furthest, 2.2f);
    EXPECT_FALSE(is_dead(go));
}

namespace
{
// the player's falls: 6 m, between its safe (3 m) and lethal (8 m) heights
constexpr float PLAYER_FALL_HEIGHT = 6.0f;

class PlayerFallTest : public FallTest
{
protected:
    Player* player_in_the_air(float height)
    {
        auto* scene = sandbox();
        auto* player = scene->build().player(0.0f, 0.0f);
        scene->snap_camera_to_player();
        step(SETTLE_FRAMES);
        set_height(player->get_game_object(), height);
        return player;
    }

    // steps until the player is back on the ground; with `roll`, a dodge is pressed once its feet are this close to
    // the floor
    bool fall_and_land(Player* player, float roll_at_height = -1.0f)
    {
        bool rolled = false;
        bool airborne = false;
        set_input({});
        for (int frame = 0; frame < 240; frame++)
        {
            PlayerInput input{};
            const float height = engineGameObjectGetTransformComponent(player->get_game_object()).position[1];
            if (roll_at_height > 0.0f && airborne && !rolled && height < roll_at_height)
            {
                input.dodge = true;
                rolled = true;
            }
            set_input(input);
            step();
            airborne = airborne || player->movement_mode() == MovementMode::eAirborne;
            if (airborne && player->movement_mode() == MovementMode::eGrounded)
            {
                set_input({});
                return true;
            }
        }
        return false;
    }
};
}  // namespace

// a fall from 6 m costs the player about 60% of its HP (3 m is safe, 8 m kills)
TEST_F(PlayerFallTest, FallFromSixMetresHurts)
{
    auto* player = player_in_the_air(PLAYER_FALL_HEIGHT);
    const auto max_hp = player->get_max_hp();
    ASSERT_EQ(player->get_hp(), max_hp);

    ASSERT_TRUE(fall_and_land(player));
    const float lost = static_cast<float>(max_hp - player->get_hp()) / static_cast<float>(max_hp);
    EXPECT_NEAR(lost, 0.6f, 0.05f);
    EXPECT_FALSE(player->is_dead());
}

// a dodge pressed just before landing is a roll: half the damage
TEST_F(PlayerFallTest, RollHalvesTheFallDamage)
{
    auto* player = player_in_the_air(PLAYER_FALL_HEIGHT);
    const auto max_hp = player->get_max_hp();

    // about 0.1 s before landing at the speed of a 6 m fall
    ASSERT_TRUE(fall_and_land(player, 1.0f));
    const float lost = static_cast<float>(max_hp - player->get_hp()) / static_cast<float>(max_hp);
    EXPECT_NEAR(lost, 0.3f, 0.05f);
}

// a fall from the safe height or lower costs nothing
TEST_F(PlayerFallTest, SafeDropCostsNothing)
{
    auto* player = player_in_the_air(2.8f);
    ASSERT_TRUE(fall_and_land(player));
    EXPECT_EQ(player->get_hp(), player->get_max_hp());
}

// the plunge is an attack: its landing costs nothing from the same height
TEST_F(PlayerFallTest, PlungeFromTheSameHeightCostsNothing)
{
    auto* scene = sandbox();
    scene->build().obstacle(glm::vec3(0.0f, PLAYER_FALL_HEIGHT * 0.5f, 0.0f), glm::vec3(4.0f, PLAYER_FALL_HEIGHT, 4.0f));
    auto* player = scene->build().player(0.0f, 0.0f);
    set_height(player->get_game_object(), PLAYER_FALL_HEIGHT + 0.05f);
    auto* enemy = scene->build().enemy(5.0f, 0.0f, FACING_PLUS_X);
    ASSERT_NE(enemy, nullptr);
    scene->snap_camera_to_player();
    step(SETTLE_FRAMES);
    const auto max_hp = player->get_max_hp();

    PlayerInput right{};
    right.right = true;
    ASSERT_TRUE(step_until(right, 120, [&] { return player->execution_target() != ENGINE_INVALID_GAME_OBJECT_ID; }));
    PlayerInput plunge = right;
    plunge.attack = true;
    set_input(plunge);
    step();
    bool executed = false;
    ASSERT_TRUE(step_until({}, 90, [&] { executed = executed || player->movement_mode() == MovementMode::eExecuting; return executed && player->movement_mode() == MovementMode::eGrounded; }));
    EXPECT_TRUE(is_dead(enemy->get_game_object()));
    EXPECT_EQ(player->get_hp(), max_hp);
}

namespace
{
class MomentumTest : public FallTest
{
protected:
    // the player behind an unaware enemy, executes it: momentum from the execution
    Player* execute_one()
    {
        auto* scene = sandbox();
        auto* player = scene->build().player(0.0f, -1.0f);
        auto* enemy = scene->build().enemy(0.0f, 0.0f, FACING_PLUS_Z);
        EXPECT_NE(enemy, nullptr);
        scene->snap_camera_to_player();
        step(SETTLE_FRAMES);
        EXPECT_EQ(player->momentum(), 0.0f);
        PlayerInput attack{};
        attack.attack = true;
        set_input(attack);
        step();
        set_input({});
        EXPECT_TRUE(step_until({}, 90, [&] { return player->movement_mode() == MovementMode::eGrounded && is_dead(enemy->get_game_object()); }));
        step(10);
        return player;
    }

    // metres per second the player runs at, once up to speed, along -x (away from everything)
    float run_speed(Player* player)
    {
        PlayerInput left{};
        left.left = true;
        set_input(left);
        step(40);
        const auto start = engineGameObjectGetTransformComponent(player->get_game_object());
        step(30);
        const auto end = engineGameObjectGetTransformComponent(player->get_game_object());
        set_input({});
        return glm::distance(glm::vec2(start.position[0], start.position[2]), glm::vec2(end.position[0], end.position[2])) / (30.0f * scenario::FIXED_DT_MS / 1000.0f);
    }
};
}  // namespace

// an execution fills momentum, and the player then runs faster than its tuned speed
TEST_F(MomentumTest, ExecutionFillsMomentumAndThePlayerRunsFaster)
{
    auto* player = execute_one();
    const auto tuning = project_c::default_momentum_tuning();
    EXPECT_NEAR(player->momentum(), tuning.ground_execution_fill, 0.01f);

    const float speed = run_speed(player);
    const float base = player->tuning().move_speed;
    EXPECT_NEAR(speed, base * (1.0f + player->momentum() * tuning.speed_bonus), 0.05f);
    EXPECT_GT(speed, base * 1.04f);
}

// out of combat it holds
TEST_F(MomentumTest, MomentumHoldsOutOfCombat)
{
    auto* player = execute_one();
    const float filled = player->momentum();
    ASSERT_GT(filled, 0.0f);
    step(180);
    EXPECT_EQ(player->momentum(), filled);
    EXPECT_FALSE(player->is_momentum_draining());
}

// on the ground with an alert enemy around it drains
TEST_F(MomentumTest, GroundCombatDrainsMomentum)
{
    auto* player = execute_one();
    const float filled = player->momentum();
    ASSERT_GT(filled, 0.0f);
    // an alert enemy 6 m away
    auto* enemy = scene_->build().enemy(6.0f, -1.0f, FACING_MINUS_X);
    ASSERT_NE(enemy, nullptr);
    step(2);
    enemy->receive_attack_hit(0, 0.0f);
    step(30);
    EXPECT_TRUE(player->is_momentum_draining());
    EXPECT_LT(player->momentum(), filled - 0.05f);
}

// kicked off a step lower than the fall damage starts at, it lands unhurt and alerted (the kick's alert isn't lost)
TEST_F(ShoveTest, EnemyKickedOffALowStepLandsAlert)
{
    constexpr float STEP_HEIGHT = 0.8f;
    auto* enemy = kick_setup(STEP_HEIGHT);
    ASSERT_NE(enemy, nullptr);
    ASSERT_EQ(enemy->awareness().state(), Awareness::eUnaware);

    press_shove();
    bool fell = false;
    ASSERT_TRUE(step_until({}, 120, [&] { fell = fell || enemy->is_falling(); return fell && !enemy->is_falling(); }));
    EXPECT_EQ(enemy->hp, enemy->get_max_hp());
    EXPECT_FALSE(enemy->is_knocked_down());
    bool alerted = false;
    step_until({}, 10, [&] { alerted = alerted || enemy->awareness().state() == Awareness::eAlert; return alerted; });
    EXPECT_TRUE(alerted);

    // the kick is settled: a later fall death of its own is no kick kill (it fills no momentum)
    const auto go = enemy->get_game_object();
    const float momentum = player_->momentum();
    set_height(go, ROOF_HEIGHT + 1.0f);
    ASSERT_TRUE(step_until({}, 120, [&] { return is_dead(go); }));
    EXPECT_EQ(player_->momentum(), momentum);
}

// a knocked-down enemy kicked only slides: it lies on and stands up as it would have (no stagger in between)
TEST_F(ShoveTest, KickingAKnockedDownEnemyKeepsItsStandUp)
{
    auto* enemy = kick_setup(0.0f);
    ASSERT_NE(enemy, nullptr);
    const float knockdown_seconds = project_c::default_execution_tuning().knockdown_seconds;
    enemy->knock_down(knockdown_seconds);
    step(10);
    ASSERT_TRUE(enemy->is_knocked_down());

    press_shove();
    bool staggered = false;
    bool stood_up = false;
    const auto frames = static_cast<std::uint32_t>((knockdown_seconds + 4.0f) * 60.0f);
    step_until({}, frames, [&]
        {
            staggered = staggered || enemy->is_staggered();
            stood_up = !enemy->is_knocked_down();
            return stood_up;
        });
    EXPECT_FALSE(staggered);
    EXPECT_TRUE(stood_up);
    // it gets up through its stand-up (no knockdown time left over)
    step(120);
    EXPECT_FALSE(enemy->is_knocked_down());
    EXPECT_FALSE(enemy->is_staggered());
}
