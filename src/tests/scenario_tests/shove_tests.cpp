#include "sandbox_scene.h"

#include <app.h>
#include <scripts/enemy_script.h>
#include <scripts/game_components.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

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
