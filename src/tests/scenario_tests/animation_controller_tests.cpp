#include "test_models.h"

#include <gtest/gtest.h>

// Animation controllers are created and owned by the engine: one per game object with an animation controller
// component and a skin, playing the model's shared animation set, and gone with the component. The animation API
// acts on a game object's controller and fails cleanly when there is none. The model is skinned_arm.gltf, which has
// the clips "Wave" (1 s) and "Idle" (0.5 s).
namespace
{
using AnimationControllerTest = scenario::ModelTest;
using scenario::TestModel;

// whether the game object has a controller: only then does the animation API answer
bool has_controller(engine_game_object_t go)
{
    float duration = 0.0f;
    return engineAnimationControllerAnimationGetDuration(go, "Idle", &duration) == ENGINE_RESULT_CODE_OK;
}

TEST_F(AnimationControllerTest, EachInstancePlaysTheModelsSharedAnimationSet)
{
    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());
    const auto first = arm.instantiate();
    const auto second = arm.instantiate();
    step();

    const auto animation_set = engineAnimationSetGetByName(arm.asset("animations").c_str());
    ASSERT_NE(animation_set, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineGameObjectGetAnimationControllerComponent(first).animations, animation_set);
    EXPECT_EQ(engineGameObjectGetAnimationControllerComponent(second).animations, animation_set);

    // one controller each: playing on one doesn't play on the other
    ASSERT_TRUE(engineAnimationControllerAnimationPlay(first, "Wave", 0));
    step();
    EXPECT_TRUE(engineAnimationControllerIsAnimationPlaying(first, "Wave"));
    EXPECT_FALSE(engineAnimationControllerIsAnimationPlaying(second, "Wave"));
}

TEST_F(AnimationControllerTest, ClipsHaveTheirDurations)
{
    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());
    const auto go = arm.instantiate();
    step();

    float duration = 0.0f;
    ASSERT_EQ(engineAnimationControllerAnimationGetDuration(go, "Wave", &duration), ENGINE_RESULT_CODE_OK);
    EXPECT_NEAR(duration, 1.0f, 1e-4f);
    ASSERT_EQ(engineAnimationControllerAnimationGetDuration(go, "Idle", &duration), ENGINE_RESULT_CODE_OK);
    EXPECT_NEAR(duration, 0.5f, 1e-4f);
    EXPECT_NE(engineAnimationControllerAnimationGetDuration(go, "No_Such_Clip", &duration), ENGINE_RESULT_CODE_OK);
    EXPECT_FALSE(engineAnimationControllerAnimationPlay(go, "No_Such_Clip", 0));
}

TEST_F(AnimationControllerTest, ControllerGoesAwayWithItsComponentOrSkin)
{
    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());
    const auto with_component = arm.instantiate();
    const auto with_skin = arm.instantiate();
    step();
    ASSERT_TRUE(has_controller(with_component));
    ASSERT_TRUE(has_controller(with_skin));

    engineGameObjectRemoveAnimationControllerComponent(with_component);
    engineGameObjectRemoveSkinComponent(with_skin);
    step();
    EXPECT_FALSE(has_controller(with_component));
    EXPECT_FALSE(has_controller(with_skin));
    // calls on a game object without a controller fail instead of crashing
    EXPECT_FALSE(engineAnimationControllerAnimationPlay(with_component, "Idle", 0));

    // a component without an animation set has no controller either
    auto ac = engineGameObjectAddAnimationControllerComponent(with_component);
    engineGameObjectUpdateAnimationControllerComponent(with_component, &ac);
    step();
    EXPECT_FALSE(has_controller(with_component));

    // giving it the set again creates one
    ac.animations = engineAnimationSetGetByName(arm.asset("animations").c_str());
    engineGameObjectUpdateAnimationControllerComponent(with_component, &ac);
    step();
    EXPECT_TRUE(has_controller(with_component));
}

TEST_F(AnimationControllerTest, DestroyedGameObjectHasNoController)
{
    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());
    const auto go = arm.instantiate();
    step();
    ASSERT_TRUE(has_controller(go));
    engineGameObjectDestroy(go);
    EXPECT_FALSE(has_controller(go));
    EXPECT_FALSE(engineAnimationControllerAnimationPlay(go, "Idle", 0));
}

TEST_F(AnimationControllerTest, LayersAreSetUpOnTheComponent)
{
    const TestModel arm("skinned_arm.gltf");
    ASSERT_TRUE(arm.loaded());
    const auto go = arm.instantiate();
    step();

    // a new component has one override layer: the controller has exactly the layers the component describes
    auto ac = engineGameObjectGetAnimationControllerComponent(go);
    ASSERT_EQ(ac.layers_count, 1u);
    EXPECT_TRUE(engineAnimationControllerLayerSetWeight(go, 0, 0.5f));
    EXPECT_FALSE(engineAnimationControllerLayerSetWeight(go, 1, 0.5f));

    // another layer, described as data on the component, with the same animation set: the controller is reconfigured
    ac.layers_count = 2;
    ac.layers[1].mode = ENGINE_ANIMATION_LAYER_MODE_OVERRIDE;
    ac.layers[1].weight = 1.0f;
    engineGameObjectUpdateAnimationControllerComponent(go, &ac);
    step();
    EXPECT_TRUE(engineAnimationControllerLayerSetWeight(go, 1, 0.5f));
    EXPECT_FALSE(engineAnimationControllerLayerSetWeight(go, 2, 0.5f));
    EXPECT_TRUE(has_controller(go));
}
}  // namespace
