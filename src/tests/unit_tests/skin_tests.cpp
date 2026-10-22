#include "skin.h"
#include "animation_controller.h"
#include "components_internals/animation_controller_internal_component.h"

#include <entt/entt.hpp>

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

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

    // an animation set with one clip, as a prefab builds it
    static std::shared_ptr<const engine::AnimationLibrary> animation_set(const engine::Skin& skin, const engine::AnimationClipDesc& clip)
    {
        auto library = std::make_shared<engine::AnimationLibrary>(skin);
        EXPECT_TRUE(library->add_animation(clip, skin));
        return library;
    }

    // the component's layer setup: one override layer, plus a second one masked by `joint_weights` if given
    static engine_animation_controller_component_t layers(std::vector<std::pair<const char*, float>> second_layer_joint_weights = {})
    {
        engine_animation_controller_component_t component{};
        component.layers_count = second_layer_joint_weights.empty() ? 1 : 2;
        component.layers[0].mode = ENGINE_ANIMATION_LAYER_MODE_OVERRIDE;
        component.layers[0].weight = 1.0f;
        component.layers[1].mode = ENGINE_ANIMATION_LAYER_MODE_OVERRIDE;
        component.layers[1].weight = 1.0f;
        component.layers[1].joint_weights_count = static_cast<std::uint32_t>(second_layer_joint_weights.size());
        for (std::size_t i = 0; i < second_layer_joint_weights.size(); i++)
        {
            std::strncpy(component.layers[1].joint_weights[i].joint_name, second_layer_joint_weights[i].first, ENGINE_ANIMATION_JOINT_NAME_MAX_LENGTH - 1);
            component.layers[1].joint_weights[i].weight = second_layer_joint_weights[i].second;
        }
        return component;
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

    engine::AnimationController controller(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    ASSERT_TRUE(controller.configure_layers(layers()));
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

    engine::AnimationController controller_a(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    engine::AnimationController controller_b(&skin, animation_set(skin, raise_hand_clip(-2.0f)));
    ASSERT_TRUE(controller_a.configure_layers(layers()));
    ASSERT_TRUE(controller_b.configure_layers(layers()));
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
TEST_F(SkinTest, ControllersAreFreedWithTheirGameObjects)
{
    // the scene owns each controller in an internal component, so destroying the game object (or removing the
    // component) frees it: reloading a scene doesn't leak controllers
    const engine::Skin skin(desc_, root_);
    const auto live_before = engine::AnimationController::get_live_count();
    {
        entt::registry registry;
        std::vector<entt::entity> entities;
        for (int i = 0; i < 3; i++)
        {
            const auto e = registry.create();
            registry.emplace<engine::animation_controller_internal_component_t>(e, std::make_unique<engine::AnimationController>(&skin, std::make_shared<engine::AnimationLibrary>(skin)));
            entities.push_back(e);
        }
        EXPECT_EQ(engine::AnimationController::get_live_count(), live_before + 3);

        registry.destroy(entities[0]);
        registry.remove<engine::animation_controller_internal_component_t>(entities[1]);
        EXPECT_EQ(engine::AnimationController::get_live_count(), live_before + 1);
    }
    // the registry (scene) is gone
    EXPECT_EQ(engine::AnimationController::get_live_count(), live_before);
}

TEST_F(SkinTest, ControllerHasOnlyTheLayersItsComponentDescribes)
{
    const engine::Skin skin(desc_, root_);
    engine::AnimationController controller(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    EXPECT_FALSE(controller.has_layer(0));
    ASSERT_TRUE(controller.configure_layers(layers({ { "root", 1.0f } })));
    EXPECT_TRUE(controller.has_layer(0));
    EXPECT_TRUE(controller.has_layer(1));
    EXPECT_FALSE(controller.has_layer(2));
    EXPECT_FALSE(controller.play("raise_hand", 2));
}

TEST_F(SkinTest, JointWeightsMaskTheLayer)
{
    const engine::Skin skin(desc_, root_);
    auto masked_pose = skin.make_rest_pose();
    auto unmasked_pose = skin.make_rest_pose();

    // the clip plays on layer 1 only; masked, the hand doesn't follow it
    engine::AnimationController masked(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    engine::AnimationController unmasked(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    ASSERT_TRUE(masked.configure_layers(layers({ { "root", 1.0f }, { "hand", 0.0f } })));
    ASSERT_TRUE(unmasked.configure_layers(layers({ { "root", 1.0f } })));
    ASSERT_TRUE(masked.play("raise_hand", 1));
    ASSERT_TRUE(unmasked.play("raise_hand", 1));
    masked.update(500.0f, masked_pose);
    unmasked.update(500.0f, unmasked_pose);

    EXPECT_NEAR(hand_height(skin, masked_pose), 1.0f, 1e-4f);
    EXPECT_NEAR(hand_height(skin, unmasked_pose), 3.0f, 1e-4f);
}

TEST_F(SkinTest, ReconfiguringLayersKeepsAnimationEvents)
{
    const engine::Skin skin(desc_, root_);
    auto pose = skin.make_rest_pose();
    engine::AnimationController controller(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    ASSERT_TRUE(controller.configure_layers(layers()));

    int fired = 0;
    engine_animation_event_t ev{};
    ev.trigger_time = 0.1f;
    ev.user_data = &fired;
    ev.fn_ptr = [](const engine_animation_event_info_t*, void* user_data) { (*static_cast<int*>(user_data))++; };
    ASSERT_TRUE(controller.add_event("raise_hand", ev).first);

    // e.g. a layer edited in the editor
    ASSERT_TRUE(controller.configure_layers(layers()));
    ASSERT_TRUE(controller.play("raise_hand", 0));
    controller.update(500.0f, pose);
    EXPECT_EQ(fired, 1);
}

TEST_F(SkinTest, EventIdsBelongToTheirController)
{
    // ids are unique in a controller and never the invalid handle; another controller numbers its own events, so
    // the ids don't depend on what else added events before
    const engine::Skin skin(desc_, root_);
    engine::AnimationController first(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    engine::AnimationController second(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    engine_animation_event_t ev{};
    ev.trigger_time = 0.1f;
    ev.fn_ptr = [](const engine_animation_event_info_t*, void*) {};

    const auto [a_added, a] = first.add_event("raise_hand", ev);
    const auto [b_added, b] = first.add_event("raise_hand", ev);
    const auto [c_added, c] = second.add_event("raise_hand", ev);
    ASSERT_TRUE(a_added && b_added && c_added);
    EXPECT_NE(a, static_cast<std::uint32_t>(ENGINE_INVALID_OBJECT_HANDLE));
    EXPECT_NE(a, b);
    EXPECT_EQ(c, a);

    // an id removes its own event once
    EXPECT_TRUE(first.remove_event("raise_hand", a));
    EXPECT_FALSE(first.remove_event("raise_hand", a));
    EXPECT_TRUE(first.remove_event("raise_hand", b));
    EXPECT_FALSE(first.add_event("missing", ev).first);
}

TEST_F(SkinTest, CrossFadeOnAnIdleLayerStartsFromTheRestPose)
{
    // e.g. a character's first animation: there is nothing to fade from, so the fade starts at the rest pose,
    // not at an unsampled (zero, so zero scale) pose
    const engine::Skin skin(desc_, root_);
    auto pose = skin.make_rest_pose();
    engine::AnimationController controller(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    ASSERT_TRUE(controller.configure_layers(layers()));
    ASSERT_TRUE(controller.cross_fade_to("raise_hand", 0, 1.0f));
    controller.update(100.0f, pose);  // ms: a tenth of the fade

    const auto hand = skin.get_model_matrix_for_joint(pose, "hand");
    EXPECT_NEAR(glm::length(glm::vec3(hand[0])), 1.0f, 1e-4f);
    EXPECT_NEAR(glm::length(glm::vec3(hand[1])), 1.0f, 1e-4f);
    EXPECT_NEAR(glm::length(glm::vec3(hand[2])), 1.0f, 1e-4f);
    EXPECT_NEAR(hand_height(skin, pose), 1.2f, 1e-3f);
}

TEST_F(SkinTest, UnknownMaskJointFailsTheLayerSetup)
{
    const engine::Skin skin(desc_, root_);
    engine::AnimationController controller(&skin, animation_set(skin, raise_hand_clip(3.0f)));
    EXPECT_FALSE(controller.configure_layers(layers({ { "tail", 1.0f } })));
    // the layers themselves still exist
    EXPECT_TRUE(controller.has_layer(1));
}
}  // namespace
