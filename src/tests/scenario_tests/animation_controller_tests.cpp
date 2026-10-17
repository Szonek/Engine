#include "sandbox_scene.h"

#include <scripts/enemy_script.h>

#include <gtest/gtest.h>

// Animation controllers are created and owned by the engine: one per game object with an animation controller
// component and a skin, playing the model's shared animation set, and gone with the component.
namespace
{
using AnimationControllerTest = scenario::GameplayTest;

TEST_F(AnimationControllerTest, EachInstancePlaysTheModelsSharedAnimationSet)
{
    auto* scene = load_sandbox_scene();
    scene->build().floor_area(-2.0f, -2.0f, 2.0f, 2.0f);
    auto* first = scene->build().enemy(-1.0f, 1.0f);
    auto* second = scene->build().enemy(1.0f, 1.0f);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    step();

    const auto animation_set = engineAnimationSetGetByName("Skeleton_Warrior.glb#animations");
    ASSERT_NE(animation_set, ENGINE_INVALID_OBJECT_HANDLE);
    const auto first_go = first->get_game_object();
    const auto second_go = second->get_game_object();
    EXPECT_EQ(engineGameObjectGetAnimationControllerComponent(first_go).animations, animation_set);
    EXPECT_EQ(engineGameObjectGetAnimationControllerComponent(second_go).animations, animation_set);

    // one controller each, already playing (the enemy script starts its idle animation)
    auto* first_controller = engineGameObjectGetAnimationController(first_go);
    auto* second_controller = engineGameObjectGetAnimationController(second_go);
    ASSERT_NE(first_controller, nullptr);
    ASSERT_NE(second_controller, nullptr);
    EXPECT_NE(first_controller, second_controller);
}

TEST_F(AnimationControllerTest, ControllerGoesAwayWithItsComponentOrSkin)
{
    auto* scene = load_sandbox_scene();
    // bare model instances, no scripts using their controllers
    const auto with_component = scenario::app().instantiate_prefab(project_c::PREFAB_TYPE_SKELETON_WARRIOR, scene).go;
    const auto with_skin = scenario::app().instantiate_prefab(project_c::PREFAB_TYPE_SKELETON_WARRIOR, scene).go;
    step();
    ASSERT_NE(engineGameObjectGetAnimationController(with_component), nullptr);
    ASSERT_NE(engineGameObjectGetAnimationController(with_skin), nullptr);

    engineGameObjectRemoveAnimationControllerComponent(with_component);
    engineGameObjectRemoveSkinComponent(with_skin);
    step();
    EXPECT_EQ(engineGameObjectGetAnimationController(with_component), nullptr);
    EXPECT_EQ(engineGameObjectGetAnimationController(with_skin), nullptr);

    // a component without an animation set has no controller either
    auto ac = engineGameObjectAddAnimationControllerComponent(with_component);
    engineGameObjectUpdateAnimationControllerComponent(with_component, &ac);
    step();
    EXPECT_EQ(engineGameObjectGetAnimationController(with_component), nullptr);

    // giving it the set again creates one
    ac.animations = engineAnimationSetGetByName("Skeleton_Warrior.glb#animations");
    engineGameObjectUpdateAnimationControllerComponent(with_component, &ac);
    step();
    EXPECT_NE(engineGameObjectGetAnimationController(with_component), nullptr);
}

TEST_F(AnimationControllerTest, ControllerOfADestroyedGameObjectCanNoLongerBeFound)
{
    auto* scene = load_sandbox_scene();
    const auto go = scenario::app().instantiate_prefab(project_c::PREFAB_TYPE_SKELETON_WARRIOR, scene).go;
    step();
    ASSERT_NE(engineGameObjectGetAnimationController(go), nullptr);
    engineGameObjectDestroy(go);
    EXPECT_EQ(engineGameObjectGetAnimationController(go), nullptr);
}
}  // namespace
