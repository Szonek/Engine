#include "skin.h"
#include "animation_controller.h"

#include <gtest/gtest.h>

// A skin is an asset shared by every instance of a skinned model; each instance has its own pose. The skin and the
// animation controller only need ozz, so they are tested here without GL (the logger comes from atlas_tests.cpp).
namespace
{
// a two-joint skeleton: "root" at the origin and "hand" one unit above it, both with the default (identity) rotation;
// glm::identity<glm::quat>() is not an identity rotation under the engine's GLM_FORCE_QUAT_DATA_XYZW
class SkinTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        root_.name = "root";
        root_.index = 0;
        root_.translation = glm::vec3(0.0f);
        root_.children = { &hand_ };

        hand_.name = "hand";
        hand_.index = 1;
        hand_.parent = &root_;
        hand_.translation = glm::vec3(0.0f, 1.0f, 0.0f);

        desc_.name = "Rig";
        desc_.asset_id = "Test.glb#Rig";
        desc_.joint_index_map = { { "root", 0 }, { "hand", 1 } };
        desc_.inverse_bind_matrix_map = { { "root", glm::mat4(1.0f) }, { "hand", glm::mat4(1.0f) } };
    }

    // the hand moves to `height` and stays there
    static engine::AnimationClipDesc raise_hand_clip(float height)
    {
        engine::AnimationChannelDesc channel;
        channel.type = engine::AnimationChannelType::eTranslation;
        channel.joint_name = "hand";
        channel.timestamps = { 0.0f, 1.0f };
        channel.data = { 0.0f, height, 0.0f, 0.0f, height, 0.0f };

        engine::AnimationClipDesc clip;
        clip.name = "raise_hand";
        clip.duration = 1.0f;
        clip.channels = { channel };
        return clip;
    }

    static float hand_height(const engine::Skin& skin, const engine::SkinPose& pose)
    {
        return skin.get_model_matrix_for_joint(pose, "hand")[3][1];
    }

    engine::ModelNodeDesc root_;
    engine::ModelNodeDesc hand_;
    engine::SkinDesc desc_;
};

TEST_F(SkinTest, RestPoseHasTheSkeletonsJointPositions)
{
    const engine::Skin skin(desc_, root_);
    const auto pose = skin.make_rest_pose();
    EXPECT_FLOAT_EQ(hand_height(skin, pose), 1.0f);
    ASSERT_EQ(pose.skinning_matrices.size(), 2u);
    // identity inverse bind matrices: skinning matrices are the joints' model matrices
    EXPECT_FLOAT_EQ(pose.skinning_matrices[1][3][1], 1.0f);
}

TEST_F(SkinTest, InstancesOfOneSkinHaveIndependentPoses)
{
    const engine::Skin skin(desc_, root_);
    auto animated = skin.make_rest_pose();
    const auto idle = skin.make_rest_pose();

    engine::AnimationController controller(&skin);
    ASSERT_TRUE(controller.add_animation(raise_hand_clip(3.0f)));
    ASSERT_TRUE(controller.play("raise_hand", 0));
    controller.update(500.0f, animated);  // ms
    skin.compute_skinning_matrices(animated);

    EXPECT_NEAR(hand_height(skin, animated), 3.0f, 1e-4f);
    EXPECT_NEAR(animated.skinning_matrices[1][3][1], 3.0f, 1e-4f);
    // the other instance of the same skin didn't move
    EXPECT_FLOAT_EQ(hand_height(skin, idle), 1.0f);
}

TEST_F(SkinTest, TwoControllersOnOneSkinDriveTheirOwnPoses)
{
    const engine::Skin skin(desc_, root_);
    auto pose_a = skin.make_rest_pose();
    auto pose_b = skin.make_rest_pose();

    engine::AnimationController controller_a(&skin);
    engine::AnimationController controller_b(&skin);
    auto clip_b = raise_hand_clip(-2.0f);
    ASSERT_TRUE(controller_a.add_animation(raise_hand_clip(3.0f)));
    ASSERT_TRUE(controller_b.add_animation(clip_b));
    ASSERT_TRUE(controller_a.play("raise_hand", 0));
    ASSERT_TRUE(controller_b.play("raise_hand", 0));
    controller_a.update(500.0f, pose_a);
    controller_b.update(500.0f, pose_b);

    EXPECT_NEAR(hand_height(skin, pose_a), 3.0f, 1e-4f);
    EXPECT_NEAR(hand_height(skin, pose_b), -2.0f, 1e-4f);
}

TEST_F(SkinTest, UnknownJointGivesIdentityInsteadOfReadingOutOfBounds)
{
    const engine::Skin skin(desc_, root_);
    const auto pose = skin.make_rest_pose();
    EXPECT_EQ(skin.get_model_matrix_for_joint(pose, "missing"), glm::mat4(1.0f));
}
TEST_F(SkinTest, CachedLibraryForAReusedSkinHandleWithAnotherSkeletonIsDropped)
{
    const engine::Skin skin(desc_, root_);

    // another skeleton: a "finger" below the hand
    engine::ModelNodeDesc finger;
    finger.name = "finger";
    finger.index = 2;
    finger.parent = &hand_;
    finger.translation = glm::vec3(0.0f, 0.5f, 0.0f);
    hand_.children = { &finger };
    auto other_desc = desc_;
    other_desc.asset_id = "Other.glb#Rig";
    other_desc.joint_index_map["finger"] = 2;
    other_desc.inverse_bind_matrix_map["finger"] = glm::mat4(1.0f);
    const engine::Skin other_skin(other_desc, root_);

    // skin handles are atlas slots: after `skin` is destroyed, `other_skin` can get the same handle
    constexpr std::uint32_t handle = 0;
    engine::AnimationLibraryCache cache;
    const auto library = std::make_shared<const engine::AnimationLibrary>(skin);
    cache.libraries[handle] = library;

    EXPECT_EQ(cache.find(handle, skin), library);
    EXPECT_EQ(cache.find(handle, other_skin), nullptr);
    EXPECT_TRUE(cache.libraries.empty());
}
}  // namespace
