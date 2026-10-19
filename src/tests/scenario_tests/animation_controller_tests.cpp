#include "sandbox_scene.h"

#include <scripts/enemy_script.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

// Animation controllers are created and owned by the engine: one per game object with an animation controller
// component and a skin, playing the model's shared animation set, and gone with the component. The animation API
// acts on a game object's controller and fails cleanly when there is none.
namespace
{
using AnimationControllerTest = scenario::GameplayTest;

// a bare skeleton (no script driving its animations)
engine_game_object_t spawn_skeleton(engine::IScene* scene)
{
    return scenario::app().instantiate_prefab(project_c::PREFAB_TYPE_SKELETON_WARRIOR, scene).go;
}

// whether the game object has a controller: only then does the animation API answer
bool has_controller(engine_game_object_t go)
{
    float duration = 0.0f;
    return engineAnimationControllerAnimationGetDuration(go, "Idle", &duration) == ENGINE_RESULT_CODE_OK;
}

TEST_F(AnimationControllerTest, EachInstancePlaysTheModelsSharedAnimationSet)
{
    auto* scene = load_sandbox_scene();
    const auto first = spawn_skeleton(scene);
    const auto second = spawn_skeleton(scene);
    step();

    const auto animation_set = engineAnimationSetGetByName("Skeleton_Warrior.glb#animations");
    ASSERT_NE(animation_set, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineGameObjectGetAnimationControllerComponent(first).animations, animation_set);
    EXPECT_EQ(engineGameObjectGetAnimationControllerComponent(second).animations, animation_set);

    // one controller each: playing on one doesn't play on the other
    ASSERT_TRUE(engineAnimationControllerAnimationPlay(first, "Running_A", 0));
    step();
    EXPECT_TRUE(engineAnimationControllerIsAnimationPlaying(first, "Running_A"));
    EXPECT_FALSE(engineAnimationControllerIsAnimationPlaying(second, "Running_A"));
}

TEST_F(AnimationControllerTest, ControllerGoesAwayWithItsComponentOrSkin)
{
    auto* scene = load_sandbox_scene();
    const auto with_component = spawn_skeleton(scene);
    const auto with_skin = spawn_skeleton(scene);
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
    ac.animations = engineAnimationSetGetByName("Skeleton_Warrior.glb#animations");
    engineGameObjectUpdateAnimationControllerComponent(with_component, &ac);
    step();
    EXPECT_TRUE(has_controller(with_component));
}

TEST_F(AnimationControllerTest, DestroyedGameObjectHasNoController)
{
    auto* scene = load_sandbox_scene();
    const auto go = spawn_skeleton(scene);
    step();
    ASSERT_TRUE(has_controller(go));
    engineGameObjectDestroy(go);
    EXPECT_FALSE(has_controller(go));
    EXPECT_FALSE(engineAnimationControllerAnimationPlay(go, "Idle", 0));
}

TEST_F(AnimationControllerTest, EnemiesAnimateThroughTheirGameObject)
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

TEST_F(AnimationControllerTest, PlayerLayersComeFromItsComponent)
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
