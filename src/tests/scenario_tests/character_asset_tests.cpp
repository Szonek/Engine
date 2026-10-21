#include "sandbox_scene.h"

#include <scripts/enemy_script.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

// The game's characters use the engine's assets as intended: their weapons are textured, and their scripts drive the
// animation controller of their own game object. These run on the game's real prefabs and test the scripts; the
// engine's behaviour has its own tests on generated models (prefab_tests, asset_name_tests, animation_controller_tests).
namespace
{
using CharacterAssetTest = scenario::GameplayTest;

TEST_F(CharacterAssetTest, PlayerWeaponUsesTheRogueTexture)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-1.0f, -1.0f, 1.0f, 1.0f);
    scene->build().player(0.0f, 0.0f);
    step();

    const auto* weapon = scene->find_script<project_c::Weapon>();
    ASSERT_NE(weapon, nullptr);
    const auto material = engineGameObjectGetMaterialComponent(weapon->get_game_object());
    EXPECT_EQ(material.data.pong.diffuse_texture, engineTextured2DGetByName("Rogue.glb#rogue_texture"));
    EXPECT_NE(material.data.pong.diffuse_texture, ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(CharacterAssetTest, PlayerWeaponHasItsSizeFromTheFirstFrame)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-1.0f, -1.0f, 1.0f, 1.0f);
    scene->build().player(0.0f, 0.0f);
    step();
    const auto* weapon = scene->find_script<project_c::Weapon>();
    ASSERT_NE(weapon, nullptr);

    // the weapon hangs off the hand joint, which the player's first animation (a cross-fade) moves
    const auto world_scale = [go = weapon->get_game_object()]()
    {
        float matrix[16]{};
        EXPECT_EQ(engineGameObjectGetWorldTransform(go, matrix), ENGINE_RESULT_CODE_OK);
        return std::sqrt(matrix[0] * matrix[0] + matrix[1] * matrix[1] + matrix[2] * matrix[2]);
    };
    std::vector<float> spawn_scales;
    for (int frame = 0; frame < 15; frame++)
    {
        spawn_scales.push_back(world_scale());
        step();
    }
    const float settled_scale = world_scale();
    ASSERT_GT(settled_scale, 0.0f);
    for (const float scale : spawn_scales)
    {
        EXPECT_NEAR(scale, settled_scale, settled_scale * 0.01f);
    }
}

TEST_F(CharacterAssetTest, EnemiesAnimateThroughTheirGameObject)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-3.0f, -3.0f, 3.0f, 3.0f);
    scene->build().player(0.0f, -2.0f);
    auto* enemy = scene->build().enemy(0.0f, 2.0f);
    ASSERT_NE(enemy, nullptr);
    step(10);
    // the enemy script drives the controller on its own game object: idle, running or attacking, depending on its state
    const auto go = enemy->get_game_object();
    const bool animating = engineAnimationControllerIsAnimationPlaying(go, "Idle")
        || engineAnimationControllerIsAnimationPlaying(go, "Running_A")
        || engineAnimationControllerIsAnimationPlaying(go, "1H_Melee_Attack_Slice_Diagonal")
        || engineAnimationControllerIsAnimationPlaying(go, "1H_Melee_Attack_Slice_Horizontal");
    EXPECT_TRUE(animating);
}

TEST_F(CharacterAssetTest, PlayerLayersComeFromItsComponent)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-1.0f, -1.0f, 1.0f, 1.0f);
    auto* player = scene->build().player(0.0f, 0.0f);
    ASSERT_NE(player, nullptr);
    step();

    // locomotion + an upper-body combat layer, described as data on the component
    const auto go = player->get_game_object();
    const auto ac = engineGameObjectGetAnimationControllerComponent(go);
    ASSERT_EQ(ac.layers_count, 2u);
    EXPECT_STREQ(ac.layers[0].name, "locomotion");
    EXPECT_STREQ(ac.layers[1].name, "combat");
    EXPECT_TRUE(ac.layers[0].looping);
    EXPECT_EQ(ac.layers[1].joint_weights_count, 4u);
    EXPECT_STREQ(ac.layers[1].joint_weights[3].joint_name, "spine");
    // the controller has exactly those layers
    EXPECT_TRUE(engineAnimationControllerLayerSetWeight(go, 1, 1.0f));
    EXPECT_FALSE(engineAnimationControllerLayerSetWeight(go, 2, 1.0f));

    // updating the component with the same animation set keeps the controller: the player still animates
    engineGameObjectUpdateAnimationControllerComponent(go, &ac);
    step(5);
    EXPECT_TRUE(engineAnimationControllerLayerSetWeight(go, 1, 1.0f));
}
}  // namespace
