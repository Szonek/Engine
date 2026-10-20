#include "test_models.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

// Prefabs are engine assets: a model file with the assets created from it. Instantiating one creates the model's game
// objects in the active scene. Prefabs are reference counted (the loaders and the instances hold them), and the last
// release destroys the assets. The models are the generated ones in assets/models/test_fixtures.
namespace
{
using PrefabTest = scenario::ModelTest;
using scenario::TestModel;

constexpr float TOLERANCE = 1e-5f;

TEST_F(PrefabTest, PrefabIsNamedByItsModelFile)
{
    const TestModel crate("crate.gltf");
    ASSERT_TRUE(crate.loaded());
    EXPECT_EQ(enginePrefabGetByName("crate.gltf"), crate.prefab());
    EXPECT_EQ(enginePrefabGetByName("crate"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(enginePrefabGetByName(crate.asset("Plate").c_str()), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, InstanceIsTheModelsNodesUnderTheRoot)
{
    const TestModel crate("crate.gltf");
    ASSERT_TRUE(crate.loaded());
    const auto root = crate.instantiate();
    ASSERT_NE(root, ENGINE_INVALID_GAME_OBJECT_ID);

    EXPECT_STREQ(engineGameObjectGetNameComponent(root).name, "Crate");
    EXPECT_FALSE(engineGameObjectHasMeshComponent(root));
    ASSERT_EQ(engineGameObjectGetChildrenCount(root), 2u);
    const auto lid = child_named(root, "Lid");
    const auto base = child_named(root, "Base");
    ASSERT_NE(lid, ENGINE_INVALID_GAME_OBJECT_ID);
    ASSERT_NE(base, ENGINE_INVALID_GAME_OBJECT_ID);

    // both children draw the model's one mesh with its material
    const auto geometry = engineGeometryGetByName(crate.asset("Plate").c_str());
    ASSERT_NE(geometry, ENGINE_INVALID_OBJECT_HANDLE);
    for (const auto child : { lid, base })
    {
        EXPECT_EQ(engineGameObjectGetParentComponent(child).parent, root);
        EXPECT_EQ(engineGameObjectGetMeshComponent(child).geometry, geometry);
        const auto material = engineGameObjectGetMaterialComponent(child);
        EXPECT_EQ(material.type, ENGINE_MATERIAL_TYPE_PONG);
        EXPECT_NEAR(material.data.pong.diffuse_color[0], 0.6f, TOLERANCE);
        EXPECT_NEAR(material.data.pong.diffuse_color[1], 0.4f, TOLERANCE);
        EXPECT_NEAR(material.data.pong.diffuse_color[2], 0.2f, TOLERANCE);
        EXPECT_NEAR(material.data.pong.diffuse_color[3], 1.0f, TOLERANCE);
    }
    // the node's own transform
    EXPECT_NEAR(engineGameObjectGetTransformComponent(lid).position[1], 1.0f, TOLERANCE);
    EXPECT_NEAR(engineGameObjectGetTransformComponent(base).position[1], 0.0f, TOLERANCE);
}

TEST_F(PrefabTest, InstanceRootIsLinkedToItsPrefab)
{
    const TestModel crate("crate.gltf");
    ASSERT_TRUE(crate.loaded());
    const auto first = crate.instantiate();
    const auto second = crate.instantiate();
    ASSERT_NE(first, ENGINE_INVALID_GAME_OBJECT_ID);
    ASSERT_NE(second, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_NE(first, second);
    EXPECT_EQ(enginePrefabInstanceGetPrefab(first), crate.prefab());
    EXPECT_EQ(enginePrefabInstanceGetPrefab(second), crate.prefab());

    // only the root is linked
    EXPECT_EQ(enginePrefabInstanceGetPrefab(child_named(first, "Lid")), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(enginePrefabInstanceGetPrefab(engineGameObjectCreate()), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(enginePrefabInstanceGetPrefab(ENGINE_INVALID_GAME_OBJECT_ID), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, InstancesAreEditedIndependently)
{
    const TestModel crate("crate.gltf");
    ASSERT_TRUE(crate.loaded());
    const auto edited = crate.instantiate();
    const auto untouched = crate.instantiate();
    const auto edited_lid = child_named(edited, "Lid");
    const auto edited_base = child_named(edited, "Base");
    const auto untouched_lid = child_named(untouched, "Lid");
    const auto untouched_base = child_named(untouched, "Base");

    // one copy gets another colour, another place and no mesh on its base
    auto material = engineGameObjectGetMaterialComponent(edited_lid);
    material.data.pong.diffuse_color[0] = 1.0f;
    engineGameObjectUpdateMaterialComponent(edited_lid, &material);
    auto transform = engineGameObjectGetTransformComponent(edited);
    transform.position[0] = 5.0f;
    engineGameObjectUpdateTransformComponent(edited, &transform);
    auto mesh = engineGameObjectGetMeshComponent(edited_base);
    mesh.disable = true;
    engineGameObjectUpdateMeshComponent(edited_base, &mesh);

    EXPECT_NEAR(engineGameObjectGetMaterialComponent(edited_lid).data.pong.diffuse_color[0], 1.0f, TOLERANCE);
    EXPECT_TRUE(engineGameObjectGetMeshComponent(edited_base).disable);
    // the other copy and the shared mesh are unchanged
    EXPECT_NEAR(engineGameObjectGetMaterialComponent(untouched_lid).data.pong.diffuse_color[0], 0.6f, TOLERANCE);
    EXPECT_NEAR(engineGameObjectGetTransformComponent(untouched).position[0], 0.0f, TOLERANCE);
    EXPECT_FALSE(engineGameObjectGetMeshComponent(untouched_base).disable);
    EXPECT_EQ(engineGameObjectGetMeshComponent(edited_lid).geometry, engineGameObjectGetMeshComponent(untouched_lid).geometry);
}

TEST_F(PrefabTest, InstantiatingAnUnknownPrefabFails)
{
    EXPECT_EQ(enginePrefabInstantiate(ENGINE_INVALID_OBJECT_HANDLE), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(PrefabTest, SkinnedInstanceOwnsItsSkinAndAnimationController)
{
    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());
    const auto root = arm.instantiate();
    ASSERT_NE(root, ENGINE_INVALID_GAME_OBJECT_ID);

    // the node named like the skin owns it, and the controller of the model's animations
    EXPECT_STREQ(engineGameObjectGetNameComponent(root).name, "Rig");
    ASSERT_TRUE(engineGameObjectHasSkinComponent(root));
    EXPECT_EQ(engineGameObjectGetSkinComponent(root).skin, engineSkinGetByName(arm.asset("Rig").c_str()));
    ASSERT_TRUE(engineGameObjectHasAnimationControllerComponent(root));
    EXPECT_EQ(engineGameObjectGetAnimationControllerComponent(root).animations, engineAnimationSetGetByName(arm.asset("animations").c_str()));

    // the joints are no game objects: the instance is the mesh and what is attached to a joint
    EXPECT_EQ(engineGameObjectGetChildrenCount(root), 2u);
    EXPECT_EQ(child_named(root, "base"), ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_EQ(child_named(root, "tip"), ENGINE_INVALID_GAME_OBJECT_ID);

    // the skinned mesh is deformed by the skin owner's pose
    const auto mesh = child_named(root, "arm_mesh");
    ASSERT_NE(mesh, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_EQ(engineGameObjectGetSkinnedMeshComponent(mesh).skin, root);

    // the node which was a child of a joint follows that joint
    const auto sword = child_named(root, "sword");
    ASSERT_NE(sword, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_TRUE(engineGameObjectHasMeshComponent(sword));
    ASSERT_TRUE(engineGameObjectHasJointAttachmentComponent(sword));
    const auto attachment = engineGameObjectGetJointAttachmentComponent(sword);
    EXPECT_EQ(attachment.skin, root);
    EXPECT_STREQ(engineStringCStr(attachment.joint_name), "tip");
}

TEST_F(PrefabTest, LoadingAModelTwiceReturnsTheSamePrefab)
{
    auto first = std::make_unique<TestModel>("crate.gltf");
    auto second = std::make_unique<TestModel>("crate.gltf");
    ASSERT_TRUE(first->loaded());
    ASSERT_TRUE(second->loaded());
    EXPECT_EQ(second->prefab(), first->prefab());
    const auto geometry_id = first->asset("Plate");
    const auto geometry = engineGeometryGetByName(geometry_id.c_str());
    ASSERT_NE(geometry, ENGINE_INVALID_OBJECT_HANDLE);

    // each load is a reference: the prefab and its assets stay until both are released
    first.reset();
    EXPECT_EQ(enginePrefabGetByName("crate.gltf"), second->prefab());
    EXPECT_EQ(engineGeometryGetByName(geometry_id.c_str()), geometry);
    second.reset();
    EXPECT_EQ(enginePrefabGetByName("crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineGeometryGetByName(geometry_id.c_str()), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, InstanceKeepsItsPrefabLoaded)
{
    auto crate = std::make_unique<TestModel>("crate.gltf");
    ASSERT_TRUE(crate->loaded());
    const auto prefab = crate->prefab();
    const auto root = crate->instantiate();
    const auto children = children_of(root);
    ASSERT_FALSE(children.empty());
    const auto geometry_id = crate->asset("Plate");

    // the game is done with the prefab, its instance isn't
    crate.reset();
    EXPECT_EQ(enginePrefabGetByName("crate.gltf"), prefab);
    EXPECT_EQ(enginePrefabInstanceGetPrefab(root), prefab);
    EXPECT_NE(engineGeometryGetByName(geometry_id.c_str()), ENGINE_INVALID_OBJECT_HANDLE);

    // the instance goes away as a whole, so none of its game objects is left with the destroyed assets
    engineGameObjectDestroy(root);
    for (const auto child : children)
    {
        EXPECT_FALSE(engineGameObjectIsValid(child));
    }
    EXPECT_EQ(enginePrefabGetByName("crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineGeometryGetByName(geometry_id.c_str()), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, SceneReleasesThePrefabsOfItsInstances)
{
    auto crate = std::make_unique<TestModel>("crate.gltf");
    ASSERT_TRUE(crate->loaded());
    crate->instantiate();
    crate->instantiate();
    crate.reset();
    ASSERT_NE(enginePrefabGetByName("crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);

    // the scene is destroyed with its game objects
    TearDown();
    EXPECT_EQ(enginePrefabGetByName("crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, ModelWhichCanNotBeUsedFailsToLoad)
{
    const TestModel missing("does_not_exist.gltf");
    EXPECT_FALSE(missing.loaded());
    EXPECT_EQ(missing.prefab(), ENGINE_INVALID_OBJECT_HANDLE);

    const TestModel not_gltf("not_a_model.gltf");
    EXPECT_FALSE(not_gltf.loaded());
    EXPECT_EQ(enginePrefabGetByName("not_a_model.gltf"), ENGINE_INVALID_OBJECT_HANDLE);

    // a model has at most one skin
    const TestModel two_skins("two_skins.gltf");
    EXPECT_FALSE(two_skins.loaded());
    EXPECT_EQ(enginePrefabGetByName("two_skins.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, FailedLoadLeavesNoAssetsBehind)
{
    // the mesh is created before the skins are found to be too many
    const TestModel two_skins("two_skins.gltf");
    ASSERT_FALSE(two_skins.loaded());
    EXPECT_EQ(engineGeometryGetByName(two_skins.asset("Arm").c_str()), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineSkinGetByName(two_skins.asset("Rig").c_str()), ENGINE_INVALID_OBJECT_HANDLE);

    // two meshes with the same name: the second one can't be created, the first one is released again
    const TestModel duplicates("duplicate_meshes.gltf");
    ASSERT_FALSE(duplicates.loaded());
    EXPECT_EQ(engineGeometryGetByName(duplicates.asset("Same").c_str()), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, ReleasingAPrefabDestroysItsAssets)
{
    auto crate = std::make_unique<TestModel>("crate.gltf");
    ASSERT_TRUE(crate->loaded());
    const auto geometry_id = crate->asset("Plate");
    const auto geometry = engineGeometryGetByName(geometry_id.c_str());
    ASSERT_NE(geometry, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_GT(engineGeometryGetAttributeLimits(geometry, ENGINE_VERTEX_ATTRIBUTE_TYPE_POSITION).elements_count, 0);

    crate.reset();
    EXPECT_EQ(enginePrefabGetByName("crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineGeometryGetByName(geometry_id.c_str()), ENGINE_INVALID_OBJECT_HANDLE);

    // the names are free again
    const TestModel again("crate.gltf");
    EXPECT_TRUE(again.loaded());
    // a handle kept from before finds nothing, not the new geometry
    const auto new_geometry = engineGeometryGetByName(geometry_id.c_str());
    ASSERT_NE(new_geometry, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_NE(new_geometry, geometry);
    EXPECT_EQ(engineGeometryGetAttributeLimits(geometry, ENGINE_VERTEX_ATTRIBUTE_TYPE_POSITION).elements_count, 0);

    auto arm = std::make_unique<TestModel>("skinned_arm.gltf");
    ASSERT_TRUE(arm->loaded());
    const auto skin_id = arm->asset("Rig");
    const auto animations_id = arm->asset("animations");
    ASSERT_NE(engineSkinGetByName(skin_id.c_str()), ENGINE_INVALID_OBJECT_HANDLE);
    ASSERT_NE(engineAnimationSetGetByName(animations_id.c_str()), ENGINE_INVALID_OBJECT_HANDLE);
    arm.reset();
    EXPECT_EQ(engineSkinGetByName(skin_id.c_str()), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineAnimationSetGetByName(animations_id.c_str()), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, TextureInItsOwnFileIsSharedByTheModelsUsingIt)
{
    // an image file is named by its path, so both models use one texture, destroyed with the last of them
    constexpr auto TEXTURE = "test_fixtures/test_texture.png";
    auto first = std::make_unique<TestModel>("textured_a.gltf");
    auto second = std::make_unique<TestModel>("textured_b.gltf");
    ASSERT_TRUE(first->loaded());
    ASSERT_TRUE(second->loaded());
    const auto texture = engineTextured2DGetByName(TEXTURE);
    ASSERT_NE(texture, ENGINE_INVALID_OBJECT_HANDLE);
    for (const auto* model : { first.get(), second.get() })
    {
        const auto instance = model->instantiate();
        EXPECT_EQ(engineGameObjectGetMaterialComponent(instance).data.pong.diffuse_texture, texture);
        engineGameObjectDestroy(instance);
    }

    // the model which loaded the image first goes away: the other one still has it
    first.reset();
    EXPECT_EQ(engineTextured2DGetByName(TEXTURE), texture);
    second.reset();
    EXPECT_EQ(engineTextured2DGetByName(TEXTURE), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PrefabTest, TextureCreatedByTheGameIsSharedWithTheModelsUsingIt)
{
    // the game and the model each hold the texture: it goes away with the last of them, whichever that is
    constexpr auto TEXTURE = "test_fixtures/test_texture.png";
    const auto file = std::string(TEST_ASSETS_DIR) + "/models/" + TEXTURE;
    for (const bool game_releases_first : { false, true })
    {
        SCOPED_TRACE(game_releases_first ? "the game releases it first" : "the model is released first");
        engine_texture2d_t existing = ENGINE_INVALID_OBJECT_HANDLE;
        ASSERT_EQ(engineTexture2DCreateFromFile(file.c_str(), ENGINE_TEXTURE_COLOR_SPACE_SRGB, TEXTURE, &existing), ENGINE_RESULT_CODE_OK);

        auto model = std::make_unique<TestModel>("textured_a.gltf");
        ASSERT_TRUE(model->loaded());
        const auto instance = model->instantiate();
        EXPECT_EQ(engineGameObjectGetMaterialComponent(instance).data.pong.diffuse_texture, existing);
        engineGameObjectDestroy(instance);

        if (game_releases_first)
        {
            engineTexture2DRelease(existing);
            EXPECT_EQ(engineTextured2DGetByName(TEXTURE), existing);
            model.reset();
        }
        else
        {
            model.reset();
            EXPECT_EQ(engineTextured2DGetByName(TEXTURE), existing);
            engineTexture2DRelease(existing);
        }
        EXPECT_EQ(engineTextured2DGetByName(TEXTURE), ENGINE_INVALID_OBJECT_HANDLE);
    }
}
}  // namespace
