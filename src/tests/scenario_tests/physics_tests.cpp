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

void set_scale(engine_game_object_t go, float scale)
{
    auto tc = engineGameObjectGetTransformComponent(go);
    tc.scale[0] = scale;
    tc.scale[1] = scale;
    tc.scale[2] = scale;
    engineGameObjectUpdateTransformComponent(go, &tc);
}

void add_rigid_body(engine_game_object_t go, float mass)
{
    auto rb = engineGameObjectAddRigidBodyComponent(go);
    rb.mass = mass;
    engineGameObjectUpdateRigidBodyComponent(go, &rb);
}

void set_mass(engine_game_object_t go, float mass)
{
    auto rb = engineGameObjectGetRigidBodyComponent(go);
    rb.mass = mass;
    engineGameObjectUpdateRigidBodyComponent(go, &rb);
}

// whether the physics reported a collision between a and b in the last frame
bool collided(engine_game_object_t a, engine_game_object_t b)
{
    for (size_t i = 0; i < enginePhysicsGetNumCollisions(); i++)
    {
        const auto* collision = enginePhysicsGetCollisionDesc(i);
        const auto object_a = engineCollisionDescGetObjectA(collision);
        const auto object_b = engineCollisionDescGetObjectB(collision);
        if ((object_a == a && object_b == b) || (object_a == b && object_b == a))
        {
            return true;
        }
    }
    return false;
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
    add_rigid_body(child, 1.0f);

    step(10);

    const auto position = world_position(child);
    EXPECT_NEAR(position.x, 10.0f, 1e-3f);
    EXPECT_LT(position.y, 5.0f);
    EXPECT_NEAR(engineGameObjectGetTransformComponent(child).position[0], 0.0f, 1e-3f);
    EXPECT_EQ(collider_below(10.0f, 0.0f), child);
}

// collider sizes are in the space of their game object: they scale with it and with its parents
TEST_F(PhysicsTest, ChildColliderHasTheScaleItInheritsFromItsParent)
{
    load_scene<PhysicsScene>();
    const auto parent = create_object(0.0f, 0.0f, 0.0f);
    set_scale(parent, 2.0f);
    // world x = 2, half extents 0.25 * 2
    const auto child = create_object(1.0f, 0.0f, 0.0f);
    set_parent(child, parent);
    add_box_collider(child);

    step();

    EXPECT_EQ(collider_below(2.4f, 0.0f), child);
    EXPECT_EQ(collider_below(2.6f, 0.0f), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(PhysicsTest, ChildColliderFollowsAScaleChangeOfItsParent)
{
    load_scene<PhysicsScene>();
    const auto parent = create_object(0.0f, 0.0f, 0.0f);
    const auto child = create_object(1.0f, 0.0f, 0.0f);
    set_parent(child, parent);
    add_box_collider(child);
    step();
    ASSERT_EQ(collider_below(1.2f, 0.0f), child);
    ASSERT_EQ(collider_below(1.4f, 0.0f), ENGINE_INVALID_GAME_OBJECT_ID);

    set_scale(parent, 2.0f);
    step();

    EXPECT_EQ(collider_below(2.4f, 0.0f), child);
    EXPECT_EQ(collider_below(1.2f, 0.0f), ENGINE_INVALID_GAME_OBJECT_ID);
}

// hitboxes (triggers following a parent) detect colliders moved by their transform, e.g. the climbing player whose
// rigid body is removed: both are kinematic
TEST_F(PhysicsTest, TriggerFollowingItsParentDetectsAColliderMovedByItsTransform)
{
    load_scene<PhysicsScene>();
    const auto parent = create_object(0.0f, 0.0f, 0.0f);
    const auto trigger = create_object(1.0f, 0.0f, 0.0f);
    set_parent(trigger, parent);
    add_box_collider(trigger);
    auto trigger_collider = engineGameObjectGetColliderComponent(trigger);
    trigger_collider.is_trigger = true;
    engineGameObjectUpdateColliderComponent(trigger, &trigger_collider);
    const auto target = create_object(4.0f, 0.0f, 0.0f);
    add_box_collider(target);
    step();

    bool detected = false;
    for (int i = 1; i <= 30 && !detected; i++)
    {
        move_to(parent, i * 0.1f, 0.0f);
        move_to(target, 4.0f, i * 0.001f);
        step();
        detected = collided(trigger, target);
    }

    EXPECT_TRUE(detected);
}

TEST_F(PhysicsTest, UpdatedMassOfARigidBodyTakesEffect)
{
    load_scene<PhysicsScene>();
    const auto go = create_object(0.0f, 5.0f, 0.0f);
    add_box_collider(go);
    add_rigid_body(go, 0.0f);
    step(10);
    ASSERT_NEAR(world_position(go).y, 5.0f, 1e-4f);

    set_mass(go, 1.0f);
    step(10);

    EXPECT_LT(world_position(go).y, 4.9f);
}

// colliders without mass which never move stay static: they don't detect each other
TEST_F(PhysicsTest, StaticCollidersDontCollideWithEachOther)
{
    load_scene<PhysicsScene>();
    const auto a = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(a);
    const auto b = create_object(0.1f, 0.0f, 0.0f);
    add_box_collider(b);

    step(2);

    EXPECT_FALSE(collided(a, b));
}
}  // namespace
