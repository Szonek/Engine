#include "asset_id.h"

#include <gtest/gtest.h>

namespace
{
TEST(AssetIdTest, NamedResourceIsQualifiedBySourceFile)
{
    EXPECT_EQ(engine::make_asset_id("Rogue.glb", "Cylinder.008", "mesh", 3), "Rogue.glb#Cylinder.008");
    EXPECT_EQ(engine::make_asset_id("Rogue.glb", "rogue_texture", "texture", 0), "Rogue.glb#rogue_texture");
}

TEST(AssetIdTest, UnnamedResourceUsesKindAndIndex)
{
    EXPECT_EQ(engine::make_asset_id("Floor_Prototype.gltf", "", "mesh", 0), "Floor_Prototype.gltf#mesh_0");
    EXPECT_EQ(engine::make_asset_id("Floor_Prototype.gltf", "", "texture", 12), "Floor_Prototype.gltf#texture_12");
}

TEST(AssetIdTest, SameNameInDifferentSourcesGivesDifferentIds)
{
    EXPECT_NE(engine::make_asset_id("Rogue.glb", "rogue_texture", "texture", 0),
        engine::make_asset_id("Rogue_Hooded.glb", "rogue_texture", "texture", 0));
}

TEST(AssetIdTest, SameInputsGiveSameId)
{
    EXPECT_EQ(engine::make_asset_id("Skeleton_Warrior.glb", "Rig", "skin", 0), engine::make_asset_id("Skeleton_Warrior.glb", "Rig", "skin", 0));
}
}  // namespace
