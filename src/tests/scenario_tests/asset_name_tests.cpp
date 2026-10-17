#include "sandbox_scene.h"

#include <scripts/enemy_script.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

// Asset IDs: resources inside the game's models are named "<model file>#<name>", images in their own file by that
// file's path, so the same resource has the same name in every run and a name tells which file it comes from.
namespace
{
using AssetNameTest = scenario::GameplayTest;

TEST_F(AssetNameTest, EmbeddedTexturesAreQualifiedByTheirModelFile)
{
    EXPECT_NE(engineTextured2DGetByName("Rogue.glb#rogue_texture"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_NE(engineTextured2DGetByName("Skeleton_Warrior.glb#skeleton_texture"), ENGINE_INVALID_OBJECT_HANDLE);
    // the bare glTF image name is not an asset ID: several models embed an image with the same name
    EXPECT_EQ(engineTextured2DGetByName("rogue_texture"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(AssetNameTest, TextureInItsOwnFileIsNamedByItsPathAndShared)
{
    // referenced by every KayKit prototype model, loaded once
    EXPECT_NE(engineTextured2DGetByName("KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/prototypebits_texture.png"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineTextured2DGetByName("Box_B.gltf#prototypebits_texture"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(AssetNameTest, GeometriesAreQualifiedByTheirModelFile)
{
    EXPECT_NE(engineGeometryGetByName("Rogue.glb#Cylinder.008"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_NE(engineGeometryGetByName("Rogue.glb#Cylinder.002"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(AssetNameTest, PlayerWeaponUsesTheRogueTexture)
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
TEST_F(AssetNameTest, SkinnedInstancesShareOneSkinAsset)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-2.0f, -2.0f, 2.0f, 2.0f);
    auto* first = scene->build().enemy(-1.0f, 1.0f);
    auto* second = scene->build().enemy(1.0f, 1.0f);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    step();

    const auto skin = engineSkinGetByName("Skeleton_Warrior.glb#Rig");
    ASSERT_NE(skin, ENGINE_INVALID_OBJECT_HANDLE);
    // each enemy owns an instance (its own pose) of the one skin asset
    ASSERT_TRUE(engineGameObjectHasSkinComponent(first->get_game_object()));
    ASSERT_TRUE(engineGameObjectHasSkinComponent(second->get_game_object()));
    EXPECT_EQ(engineGameObjectGetSkinComponent(first->get_game_object()).skin, skin);
    EXPECT_EQ(engineGameObjectGetSkinComponent(second->get_game_object()).skin, skin);
}
}  // namespace
