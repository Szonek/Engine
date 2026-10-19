#include "sandbox_scene.h"

#include <gtest/gtest.h>

#include <vector>

// Prefabs are engine assets: a model file with the assets created from it. Instantiating one creates the model's game
// objects in the active scene; destroying it destroys the assets.
namespace
{
using PrefabTest = scenario::GameplayTest;

constexpr auto BARREL_FILE = "Barrel_A.gltf";
constexpr auto BARREL_DIR = "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf";
// the model's only mesh
constexpr auto BARREL_GEOMETRY = "Barrel_A.gltf#Cylinder.020";

TEST_F(PrefabTest, PrefabsAreNamedByTheirModelFile)
{
    EXPECT_NE(enginePrefabGetByName("Rogue.glb"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(enginePrefabGetByName("Rogue"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(enginePrefabGetByName("Rogue.glb#Rig"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, InstanceRootIsLinkedToItsPrefab)
{
    load_sandbox_scene();
    const auto prefab = enginePrefabGetByName("Skeleton_Warrior.glb");
    ASSERT_NE(prefab, ENGINE_INVALID_OBJECT_HANDLE);

    const auto first = enginePrefabInstantiate(prefab);
    const auto second = enginePrefabInstantiate(prefab);
    ASSERT_NE(first, ENGINE_INVALID_GAME_OBJECT_ID);
    ASSERT_NE(second, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_NE(first, second);
    EXPECT_EQ(enginePrefabInstanceGetPrefab(first), prefab);
    EXPECT_EQ(enginePrefabInstanceGetPrefab(second), prefab);
    step();

    // the root is the parent of the rest of the instance, which isn't linked itself
    const auto child = engineGameObjectGetChild(first, 0);
    ASSERT_NE(child, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_EQ(engineGameObjectGetParentComponent(child).parent, first);
    EXPECT_EQ(enginePrefabInstanceGetPrefab(child), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, GameObjectWhichIsNotAnInstanceRootHasNoPrefab)
{
    load_sandbox_scene();
    EXPECT_EQ(enginePrefabInstanceGetPrefab(engineGameObjectCreate()), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(enginePrefabInstanceGetPrefab(ENGINE_INVALID_GAME_OBJECT_ID), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, InstantiatingAnUnknownPrefabFails)
{
    load_sandbox_scene();
    EXPECT_EQ(enginePrefabInstantiate(ENGINE_INVALID_OBJECT_HANDLE), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(PrefabTest, SkinnedInstanceOwnsItsSkinAndAnimationController)
{
    load_sandbox_scene();
    const auto root = enginePrefabInstantiate(enginePrefabGetByName("Skeleton_Warrior.glb"));
    ASSERT_NE(root, ENGINE_INVALID_GAME_OBJECT_ID);
    step();

    // the skin's owner is one of the instance's game objects
    auto skin_owner = ENGINE_INVALID_GAME_OBJECT_ID;
    std::vector<engine_game_object_t> instance = { root };
    for (size_t i = 0; i < engineGameObjectGetChildrenCount(root); i++)
    {
        instance.push_back(engineGameObjectGetChild(root, i));
    }
    for (const auto go : instance)
    {
        if (engineGameObjectHasSkinComponent(go))
        {
            skin_owner = go;
        }
    }
    ASSERT_NE(skin_owner, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_EQ(engineGameObjectGetSkinComponent(skin_owner).skin, engineSkinGetByName("Skeleton_Warrior.glb#Rig"));
    EXPECT_EQ(engineGameObjectGetAnimationControllerComponent(skin_owner).animations, engineAnimationSetGetByName("Skeleton_Warrior.glb#animations"));
}

TEST_F(PrefabTest, ModelCanNotBeLoadedTwice)
{
    engine_prefab_t duplicate = ENGINE_INVALID_OBJECT_HANDLE;
    EXPECT_NE(enginePrefabCreateFromFile(ENGINE_MODEL_SPECIFICATION_GLTF_2, "Rogue.glb", "KayKit_Adventurers_1.0_FREE/Characters/gltf", &duplicate), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(duplicate, ENGINE_INVALID_OBJECT_HANDLE);
    // the first one is untouched
    EXPECT_NE(enginePrefabGetByName("Rogue.glb"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_NE(engineSkinGetByName("Rogue.glb#Rig"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, MissingModelFileFails)
{
    engine_prefab_t prefab = ENGINE_INVALID_OBJECT_HANDLE;
    EXPECT_NE(enginePrefabCreateFromFile(ENGINE_MODEL_SPECIFICATION_GLTF_2, "does_not_exist.glb", "", &prefab), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(prefab, ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, DestroyingAPrefabDestroysItsAssets)
{
    ASSERT_EQ(enginePrefabGetByName(BARREL_FILE), ENGINE_INVALID_OBJECT_HANDLE);

    engine_prefab_t prefab = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(enginePrefabCreateFromFile(ENGINE_MODEL_SPECIFICATION_GLTF_2, BARREL_FILE, BARREL_DIR, &prefab), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(enginePrefabGetByName(BARREL_FILE), prefab);
    EXPECT_NE(engineGeometryGetByName(BARREL_GEOMETRY), ENGINE_INVALID_OBJECT_HANDLE);

    enginePrefabDestroy(prefab);
    EXPECT_EQ(enginePrefabGetByName(BARREL_FILE), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineGeometryGetByName(BARREL_GEOMETRY), ENGINE_INVALID_OBJECT_HANDLE);

    // the model is loadable again, and the texture it shares with the game's models was not destroyed with it
    ASSERT_EQ(enginePrefabCreateFromFile(ENGINE_MODEL_SPECIFICATION_GLTF_2, BARREL_FILE, BARREL_DIR, &prefab), ENGINE_RESULT_CODE_OK);
    enginePrefabDestroy(prefab);
    EXPECT_NE(engineTextured2DGetByName("KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/prototypebits_texture.png"), ENGINE_INVALID_OBJECT_HANDLE);
}
}  // namespace
