#include "scenario_harness.h"

#include <iscene.h>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

// Engine physics seen through the game: colliders follow their game objects (including children) on the frame
// the objects move, not a frame later. Transforms are local to the parent, bodies are in world space.
namespace
{
class PhysicsScene : public engine::IScene
{
public:
    PhysicsScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "scenario_physics"; }
};

engine_game_object_t create_object(float x, float y, float z)
{
    const auto go = engineGameObjectCreate();
    auto tc = engineGameObjectAddTransformComponent(go);
    tc.position[0] = x;
    tc.position[1] = y;
    tc.position[2] = z;
    engineGameObjectUpdateTransformComponent(go, &tc);
    return go;
}

// 0.5 m static box collider
void add_box_collider(engine_game_object_t go)
{
    auto cc = engineGameObjectAddColliderComponent(go);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    cc.collider.box.size[0] = 0.25f;
    cc.collider.box.size[1] = 0.25f;
    cc.collider.box.size[2] = 0.25f;
    engineGameObjectUpdateColliderComponent(go, &cc);
}

void set_parent(engine_game_object_t child, engine_game_object_t parent)
{
    auto pc = engineGameObjectAddParentComponent(child);
    pc.parent = parent;
    engineGameObjectUpdateParentComponent(child, &pc);
}

void move_to(engine_game_object_t go, float x, float z)
{
    auto tc = engineGameObjectGetTransformComponent(go);
    tc.position[0] = x;
    tc.position[2] = z;
    engineGameObjectUpdateTransformComponent(go, &tc);
}

// the translation of the game object's world transform after the last scene update
glm::vec3 world_position(engine_game_object_t go)
{
    float matrix[16]{};
    EXPECT_EQ(engineGameObjectGetWorldTransform(go, matrix), ENGINE_RESULT_CODE_OK);
    return { matrix[12], matrix[13], matrix[14] };
}

// game object whose collider a vertical ray at (x, z) hits first, or ENGINE_INVALID_GAME_OBJECT_ID
engine_game_object_t collider_below(float x, float z)
{
    engine_ray_t ray{};
    ray.origin = { x, 10.0f, z };
    ray.direction = { x, -10.0f, z };  // the end point of the ray
    return enginePhysicsRayCast(nullptr, 0, &ray, 20.0f).go;
}

using PhysicsTest = scenario::ScenarioTest;

TEST_F(PhysicsTest, ChildColliderIsAtItsWorldPositionOnItsFirstFrame)
{
    load_scene<PhysicsScene>();
    const auto parent = create_object(3.0f, 0.0f, 0.0f);
    // 2 m to the right of the parent: world x = 5
    const auto child = create_object(2.0f, 0.0f, 0.0f);
    set_parent(child, parent);
    add_box_collider(child);

    step();

    EXPECT_EQ(collider_below(5.0f, 0.0f), child);
    // not at its local position
    EXPECT_EQ(collider_below(2.0f, 0.0f), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(PhysicsTest, ChildColliderFollowsItsParentOnTheSameFrame)
{
    load_scene<PhysicsScene>();
    const auto parent = create_object(0.0f, 0.0f, 0.0f);
    const auto child = create_object(2.0f, 0.0f, 0.0f);
    set_parent(child, parent);
    add_box_collider(child);
    step(2);
    ASSERT_EQ(collider_below(2.0f, 0.0f), child);

    move_to(parent, 0.0f, 4.0f);
    step();

    EXPECT_EQ(collider_below(2.0f, 4.0f), child);
    EXPECT_EQ(collider_below(2.0f, 0.0f), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(PhysicsTest, WorldTransformOfAChildIsItsParentsTimesItsOwn)
{
    load_scene<PhysicsScene>();
    const auto parent = create_object(3.0f, 1.0f, 0.0f);
    auto parent_tc = engineGameObjectGetTransformComponent(parent);
    // a quarter turn around y: the child's local +x becomes world -z
    parent_tc.rotation[1] = 0.70710678f;
    parent_tc.rotation[3] = 0.70710678f;
    engineGameObjectUpdateTransformComponent(parent, &parent_tc);
    const auto child = create_object(2.0f, 0.0f, 0.0f);
    set_parent(child, parent);

    step();

    const auto position = world_position(child);
    EXPECT_NEAR(position.x, 3.0f, 1e-4f);
    EXPECT_NEAR(position.y, 1.0f, 1e-4f);
    EXPECT_NEAR(position.z, -2.0f, 1e-4f);
    // the child's own transform stays local
    EXPECT_FLOAT_EQ(engineGameObjectGetTransformComponent(child).position[0], 2.0f);
}

TEST_F(PhysicsTest, WorldTransformOfAGameObjectWithoutTransformFails)
{
    load_scene<PhysicsScene>();
    const auto go = engineGameObjectCreate();
    float matrix[16]{};
    EXPECT_EQ(engineGameObjectGetWorldTransform(go, matrix), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(engineGameObjectGetWorldTransform(go, nullptr), ENGINE_RESULT_CODE_INVALID_ARG);
}

// the step moves a dynamic body in world space; its game object's transform gets it relative to the parent
TEST_F(PhysicsTest, DynamicChildBodyStaysWhereThePhysicsPutsIt)
{
    load_scene<PhysicsScene>();
    const auto parent = create_object(10.0f, 0.0f, 0.0f);
    const auto child = create_object(0.0f, 5.0f, 0.0f);
    set_parent(child, parent);
    add_box_collider(child);
    auto rb = engineGameObjectAddRigidBodyComponent(child);
    rb.mass = 1.0f;
    engineGameObjectUpdateRigidBodyComponent(child, &rb);

    step(10);

    const auto position = world_position(child);
    EXPECT_NEAR(position.x, 10.0f, 1e-3f);
    EXPECT_LT(position.y, 5.0f);
    EXPECT_NEAR(engineGameObjectGetTransformComponent(child).position[0], 0.0f, 1e-3f);
    EXPECT_EQ(collider_below(10.0f, 0.0f), child);
}
}  // namespace
