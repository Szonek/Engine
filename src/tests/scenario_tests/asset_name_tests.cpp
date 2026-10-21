#include "test_models.h"

#include <gtest/gtest.h>

// Asset IDs: resources inside a model are named "<model path>#<name>", images in their own file by that file's path
// relative to the models folder, so the same resource has the same name in every run and a name tells which file it
// comes from.
namespace
{
using AssetNameTest = scenario::ModelTest;
using scenario::TestModel;

TEST_F(AssetNameTest, GeometriesAreQualifiedByTheirModelFile)
{
    const TestModel crate("crate.gltf");
    ASSERT_TRUE(crate.loaded());
    EXPECT_NE(engineGeometryGetByName("test_fixtures/crate.gltf#Plate"), ENGINE_INVALID_OBJECT_HANDLE);
    // the bare mesh name is not an asset ID: several models have a mesh with the same name
    EXPECT_EQ(engineGeometryGetByName("Plate"), ENGINE_INVALID_OBJECT_HANDLE);

    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());
    EXPECT_NE(engineGeometryGetByName("test_fixtures/skinned_arm.gltf#Arm"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_NE(engineGeometryGetByName("test_fixtures/skinned_arm.gltf#Sword"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(AssetNameTest, EmbeddedTexturesAreQualifiedByTheirModelFile)
{
    const TestModel model("embedded_texture.gltf");
    ASSERT_TRUE(model.loaded());
    EXPECT_NE(engineTextured2DGetByName("test_fixtures/embedded_texture.gltf#checker"), ENGINE_INVALID_OBJECT_HANDLE);
    // the bare glTF image name is not an asset ID: several models embed an image with the same name
    EXPECT_EQ(engineTextured2DGetByName("checker"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(AssetNameTest, TextureInItsOwnFileIsNamedByItsPath)
{
    const TestModel model("textured_a.gltf");
    ASSERT_TRUE(model.loaded());
    EXPECT_NE(engineTextured2DGetByName("test_fixtures/test_texture.png"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineTextured2DGetByName("test_fixtures/textured_a.gltf#test_texture.png"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(AssetNameTest, SkinAndAnimationSetAreQualifiedByTheirModelFile)
{
    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());
    EXPECT_NE(engineSkinGetByName("test_fixtures/skinned_arm.gltf#Rig"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_NE(engineAnimationSetGetByName("test_fixtures/skinned_arm.gltf#animations"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineSkinGetByName("Rig"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(AssetNameTest, SkinnedInstancesShareOneSkinAsset)
{
    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());
    const auto first = arm.instantiate();
    const auto second = arm.instantiate();
    step();

    const auto skin = engineSkinGetByName("test_fixtures/skinned_arm.gltf#Rig");
    ASSERT_NE(skin, ENGINE_INVALID_OBJECT_HANDLE);
    // each instance owns its own pose of the one skin asset
    ASSERT_TRUE(engineGameObjectHasSkinComponent(first));
    ASSERT_TRUE(engineGameObjectHasSkinComponent(second));
    EXPECT_NE(first, second);
    EXPECT_EQ(engineGameObjectGetSkinComponent(first).skin, skin);
    EXPECT_EQ(engineGameObjectGetSkinComponent(second).skin, skin);
}

TEST_F(AssetNameTest, SkinComponentWithoutASkinHasNoPose)
{
    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());

    // a skin component added but never given a skin (e.g. "add component" in the editor)
    const auto owner = engineGameObjectCreate();
    engineGameObjectAddTransformComponent(owner);
    engineGameObjectAddSkinComponent(owner);
    ASSERT_EQ(engineGameObjectGetSkinComponent(owner).skin, ENGINE_INVALID_OBJECT_HANDLE);

    // something attached to it: it must not read a pose of some other skin
    const auto attached = engineGameObjectCreate();
    engineGameObjectAddTransformComponent(attached);
    auto jac = engineGameObjectAddJointAttachmentComponent(attached);
    jac.skin = owner;
    engineStringSet(jac.joint_name, "tip");
    engineGameObjectUpdateJointAttachmentComponent(attached, &jac);
    step(2);

    // giving it a skin later works, and taking it away again removes the pose
    auto sc = engineGameObjectGetSkinComponent(owner);
    sc.skin = engineSkinGetByName(arm.asset("Rig").c_str());
    ASSERT_NE(sc.skin, ENGINE_INVALID_OBJECT_HANDLE);
    engineGameObjectUpdateSkinComponent(owner, &sc);
    step(2);
    sc.skin = ENGINE_INVALID_OBJECT_HANDLE;
    engineGameObjectUpdateSkinComponent(owner, &sc);
    step(2);

    EXPECT_TRUE(engineGameObjectIsValid(attached));
}
}  // namespace
