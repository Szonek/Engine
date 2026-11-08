#include "scenario_harness.h"

#include <iscene.h>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

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

// static box collider, 0.5 m unless given its half extents
void add_box_collider(engine_game_object_t go, const glm::vec3& half_extents = glm::vec3(0.25f))
{
    auto cc = engineGameObjectAddColliderComponent(go);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    cc.collider.box.size[0] = half_extents.x;
    cc.collider.box.size[1] = half_extents.y;
    cc.collider.box.size[2] = half_extents.z;
    engineGameObjectUpdateColliderComponent(go, &cc);
}

void set_collider_disabled(engine_game_object_t go, bool disabled)
{
    auto cc = engineGameObjectGetColliderComponent(go);
    cc.disable = disabled;
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

void set_body_type(engine_game_object_t go, engine_rigid_body_type_t type)
{
    auto rb = engineGameObjectHasRigidBodyComponent(go) ? engineGameObjectGetRigidBodyComponent(go) : engineGameObjectAddRigidBodyComponent(go);
    rb.type = type;
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

void make_trigger(engine_game_object_t go)
{
    auto cc = engineGameObjectGetColliderComponent(go);
    cc.is_trigger = true;
    engineGameObjectUpdateColliderComponent(go, &cc);
}

// game object whose collider a vertical ray at (x, z) hits first, or ENGINE_INVALID_GAME_OBJECT_ID
engine_game_object_t collider_below(float x, float z)
{
    const float origin[3] = { x, 10.0f, z };
    const float down[3] = { 0.0f, -1.0f, 0.0f };
    engine_ray_hit_t hit{};
    return enginePhysicsRayCast(origin, down, 20.0f, nullptr, &hit) ? hit.go : ENGINE_INVALID_GAME_OBJECT_ID;
}

// a ray from x = -10 along +x (direction of length 2, not normalized) at y = z = 0
bool cast_along_x(float max_distance, const engine_physics_query_filter_t* filter, engine_ray_hit_t& hit)
{
    const float origin[3] = { -10.0f, 0.0f, 0.0f };
    const float direction[3] = { 2.0f, 0.0f, 0.0f };
    return enginePhysicsRayCast(origin, direction, max_distance, filter, &hit);
}

// the game objects overlapping a sphere at (x, y, z), nearest first
std::vector<engine_game_object_t> overlap_sphere(const glm::vec3& center, float radius, const engine_physics_query_filter_t* filter = nullptr)
{
    std::vector<engine_game_object_t> found(16, ENGINE_INVALID_GAME_OBJECT_ID);
    const auto count = enginePhysicsOverlapSphere(&center.x, radius, filter, found.data(), found.size());
    EXPECT_LE(count, found.size());
    found.resize(std::min(count, found.size()));
    return found;
}

bool contains(const std::vector<engine_game_object_t>& found, engine_game_object_t go)
{
    return std::find(found.begin(), found.end(), go) != found.end();
}

class PhysicsTest : public scenario::ScenarioTest
{
protected:
    // a trigger (hitbox) 1 m to the right of a parent which is moved toward `target`: whether they collided on the way
    bool hitbox_detects(engine_game_object_t target, std::optional<engine_rigid_body_type_t> hitbox_type = std::nullopt)
    {
        const auto parent = create_object(0.0f, 0.0f, 0.0f);
        const auto hitbox = create_object(1.0f, 0.0f, 0.0f);
        set_parent(hitbox, parent);
        add_box_collider(hitbox);
        auto collider = engineGameObjectGetColliderComponent(hitbox);
        collider.is_trigger = true;
        engineGameObjectUpdateColliderComponent(hitbox, &collider);
        if (hitbox_type)
        {
            set_body_type(hitbox, *hitbox_type);
        }
        step();

        for (int i = 1; i <= 30; i++)
        {
            move_to(parent, i * 0.1f, 0.0f);
            // the target moves too (by its transform), like the climbing player
            move_to(target, 4.0f, i * 0.001f);
            step();
            if (collided(hitbox, target))
            {
                return true;
            }
        }
        return false;
    }
};

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

// hitboxes (colliders following a parent are kinematic) detect kinematic bodies, e.g. the climbing player
TEST_F(PhysicsTest, HitboxDetectsAKinematicBody)
{
    load_scene<PhysicsScene>();
    const auto target = create_object(4.0f, 0.0f, 0.0f);
    add_box_collider(target);
    set_body_type(target, ENGINE_RIGID_BODY_TYPE_KINEMATIC);

    EXPECT_TRUE(hitbox_detects(target));
}

// the engine doesn't infer a type from movement: a collider alone moved by its transform stays static
TEST_F(PhysicsTest, ColliderMovedByItsTransformStaysStatic)
{
    load_scene<PhysicsScene>();
    const auto target = create_object(4.0f, 0.0f, 0.0f);
    add_box_collider(target);

    EXPECT_FALSE(hitbox_detects(target));
}

TEST_F(PhysicsTest, ExplicitTypeWinsOverTheParent)
{
    load_scene<PhysicsScene>();
    const auto target = create_object(4.0f, 0.0f, 0.0f);
    add_box_collider(target);
    set_body_type(target, ENGINE_RIGID_BODY_TYPE_KINEMATIC);

    // a static hitbox doesn't pair with a kinematic body
    EXPECT_FALSE(hitbox_detects(target, ENGINE_RIGID_BODY_TYPE_STATIC));
}

TEST_F(PhysicsTest, KinematicBodyIsNotMovedByGravity)
{
    load_scene<PhysicsScene>();
    const auto go = create_object(0.0f, 5.0f, 0.0f);
    add_box_collider(go);
    auto rb = engineGameObjectAddRigidBodyComponent(go);
    rb.type = ENGINE_RIGID_BODY_TYPE_KINEMATIC;
    rb.mass = 1.0f;
    engineGameObjectUpdateRigidBodyComponent(go, &rb);

    step(30);

    EXPECT_NEAR(world_position(go).y, 5.0f, 1e-4f);
}

// bodies exist as soon as their components do: a script's ray cast sees what was created earlier in the same frame
TEST_F(PhysicsTest, RayCastHitsAColliderCreatedInTheSameFrame)
{
    load_scene<PhysicsScene>();
    step();
    const auto go = create_object(2.0f, 0.0f, 0.0f);
    add_box_collider(go);

    EXPECT_EQ(collider_below(2.0f, 0.0f), go);
}

TEST_F(PhysicsTest, RayCastHitsAColliderWhereItWasMovedInTheSameFrame)
{
    load_scene<PhysicsScene>();
    const auto go = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(go);
    set_body_type(go, ENGINE_RIGID_BODY_TYPE_KINEMATIC);
    step();

    move_to(go, 3.0f, 0.0f);

    EXPECT_EQ(collider_below(3.0f, 0.0f), go);
    EXPECT_EQ(collider_below(0.0f, 0.0f), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(PhysicsTest, RayCastHitsAChildWhereItsParentWasMovedInTheSameFrame)
{
    load_scene<PhysicsScene>();
    const auto parent = create_object(0.0f, 0.0f, 0.0f);
    const auto child = create_object(1.0f, 0.0f, 0.0f);
    set_parent(child, parent);
    add_box_collider(child);
    step();

    move_to(parent, 0.0f, 4.0f);

    EXPECT_EQ(collider_below(1.0f, 4.0f), child);
    EXPECT_EQ(collider_below(1.0f, 0.0f), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(PhysicsTest, RayCastReportsTheClosestHitWithItsDistanceAndNormal)
{
    load_scene<PhysicsScene>();
    const auto near_box = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(near_box);
    const auto far_box = create_object(5.0f, 0.0f, 0.0f);
    add_box_collider(far_box);

    engine_ray_hit_t hit{};
    ASSERT_TRUE(cast_along_x(100.0f, nullptr, hit));

    EXPECT_EQ(hit.go, near_box);
    // the near face of the 0.5 m box at the origin, 9.75 m from x = -10 (the direction's length doesn't scale it)
    EXPECT_NEAR(hit.position[0], -0.25f, 1e-3f);
    EXPECT_NEAR(hit.distance, 9.75f, 1e-3f);
    EXPECT_NEAR(hit.normal[0], -1.0f, 1e-3f);
}

TEST_F(PhysicsTest, RayCastEndsAtItsMaxDistance)
{
    load_scene<PhysicsScene>();
    const auto box = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(box);

    engine_ray_hit_t hit{};
    EXPECT_FALSE(cast_along_x(9.5f, nullptr, hit));
    ASSERT_TRUE(cast_along_x(10.0f, nullptr, hit));
    EXPECT_EQ(hit.go, box);
}

TEST_F(PhysicsTest, RayCastWithAnEmptyRayHitsNothing)
{
    load_scene<PhysicsScene>();
    add_box_collider(create_object(0.0f, 0.0f, 0.0f));

    const float origin[3] = { -10.0f, 0.0f, 0.0f };
    const float zero[3] = { 0.0f, 0.0f, 0.0f };
    const float along_x[3] = { 1.0f, 0.0f, 0.0f };
    engine_ray_hit_t hit{};
    EXPECT_FALSE(enginePhysicsRayCast(origin, zero, 100.0f, nullptr, &hit));
    EXPECT_FALSE(enginePhysicsRayCast(origin, along_x, 0.0f, nullptr, &hit));
}

TEST_F(PhysicsTest, RayCastSkipsTriggersUnlessAsked)
{
    load_scene<PhysicsScene>();
    const auto trigger = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(trigger);
    make_trigger(trigger);
    const auto wall = create_object(5.0f, 0.0f, 0.0f);
    add_box_collider(wall);

    engine_ray_hit_t hit{};
    ASSERT_TRUE(cast_along_x(100.0f, nullptr, hit));
    EXPECT_EQ(hit.go, wall);

    engine_physics_query_filter_t filter{};
    filter.hit_triggers = true;
    ASSERT_TRUE(cast_along_x(100.0f, &filter, hit));
    EXPECT_EQ(hit.go, trigger);
}

TEST_F(PhysicsTest, DisabledColliderHasNoBody)
{
    load_scene<PhysicsScene>();
    const auto disabled = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(disabled);
    set_collider_disabled(disabled, true);
    const auto wall = create_object(5.0f, 0.0f, 0.0f);
    add_box_collider(wall);

    // not even a ray cast asking for triggers sees it
    engine_physics_query_filter_t filter{};
    filter.hit_triggers = true;
    engine_ray_hit_t hit{};
    ASSERT_TRUE(cast_along_x(100.0f, &filter, hit));
    EXPECT_EQ(hit.go, wall);

    // enabled again, it has its body at once
    set_collider_disabled(disabled, false);
    ASSERT_TRUE(cast_along_x(100.0f, nullptr, hit));
    EXPECT_EQ(hit.go, disabled);
}

TEST_F(PhysicsTest, NavMeshLeavesOutADisabledCollider)
{
    load_scene<PhysicsScene>();
    engineGameObjectAddNavSurfaceComponent(engineGameObjectCreate());
    // a 10 x 10 m floor with its top at y = 0, and a wall across it at x = 0 (z in [-3, 3]: paths go around it)
    const auto floor = create_object(0.0f, -0.5f, 0.0f);
    add_box_collider(floor, { 5.0f, 0.5f, 5.0f });
    const auto wall = create_object(0.0f, 1.0f, 0.0f);
    add_box_collider(wall, { 0.25f, 1.0f, 3.0f });

    const auto path_corners = []()
        {
            EXPECT_EQ(engineNavMeshBuild(), ENGINE_RESULT_CODE_OK);
            const float start[3] = { -3.0f, 0.0f, 0.0f };
            const float end[3] = { 3.0f, 0.0f, 0.0f };
            const float extents[3] = { 1.0f, 2.0f, 1.0f };
            float points[16 * 3] = {};
            size_t count = 0;
            engine_nav_path_status_t status{};
            EXPECT_EQ(engineNavFindPath(start, end, extents, points, 16, &count, &status), ENGINE_RESULT_CODE_OK);
            EXPECT_EQ(status, ENGINE_NAV_PATH_STATUS_COMPLETE);
            return count;
        };
    // around the wall
    EXPECT_GT(path_corners(), 2u);
    // straight through where it was
    set_collider_disabled(wall, true);
    EXPECT_EQ(path_corners(), 2u);
}

TEST_F(PhysicsTest, RayCastPassesThroughIgnoredGameObjects)
{
    load_scene<PhysicsScene>();
    const auto near_box = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(near_box);
    const auto far_box = create_object(5.0f, 0.0f, 0.0f);
    add_box_collider(far_box);

    engine_physics_query_filter_t filter{};
    filter.ignore = &near_box;
    filter.ignore_count = 1;
    engine_ray_hit_t hit{};
    ASSERT_TRUE(cast_along_x(100.0f, &filter, hit));
    EXPECT_EQ(hit.go, far_box);
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

// a box inside the sphere and one touching its edge (1 mm in) overlap it, one just outside doesn't
TEST_F(PhysicsTest, OverlapSphereFindsOverlappingColliders)
{
    load_scene<PhysicsScene>();
    const auto inside = create_object(0.5f, 0.0f, 0.0f);
    add_box_collider(inside);
    const auto touching = create_object(-1.249f, 0.0f, 0.0f);
    add_box_collider(touching);
    const auto outside = create_object(0.0f, 0.0f, 1.3f);
    add_box_collider(outside);
    step();

    const auto found = overlap_sphere(glm::vec3(0.0f), 1.0f);
    EXPECT_EQ(found.size(), 2u);
    EXPECT_TRUE(contains(found, inside));
    EXPECT_TRUE(contains(found, touching));
    EXPECT_FALSE(contains(found, outside));
}

TEST_F(PhysicsTest, OverlapSphereFindsAStaticAndAKinematicBody)
{
    load_scene<PhysicsScene>();
    const auto wall = create_object(0.5f, 0.0f, 0.0f);
    add_box_collider(wall);
    const auto mover = create_object(-0.5f, 0.0f, 0.0f);
    add_box_collider(mover);
    set_body_type(mover, ENGINE_RIGID_BODY_TYPE_KINEMATIC);
    step();

    const auto found = overlap_sphere(glm::vec3(0.0f), 1.0f);
    EXPECT_TRUE(contains(found, wall));
    EXPECT_TRUE(contains(found, mover));
}

TEST_F(PhysicsTest, OverlapSphereFindsACharacterController)
{
    load_scene<PhysicsScene>();
    const auto character = create_object(0.5f, 0.0f, 0.0f);
    auto controller = engineGameObjectAddCharacterControllerComponent(character);
    controller.radius = 0.3f;
    controller.height = 1.8f;
    controller.step_height = 0.3f;
    engineGameObjectUpdateCharacterControllerComponent(character, &controller);
    step();

    // the capsule stands on its origin: its side is 0.2 from the center at the height of the sphere
    const auto found = overlap_sphere(glm::vec3(0.0f, 0.9f, 0.0f), 0.3f);
    EXPECT_EQ(found, std::vector<engine_game_object_t>{ character });
}

TEST_F(PhysicsTest, OverlapSphereSkipsTriggersUnlessAsked)
{
    load_scene<PhysicsScene>();
    const auto trigger = create_object(0.5f, 0.0f, 0.0f);
    add_box_collider(trigger);
    make_trigger(trigger);
    step();

    EXPECT_TRUE(overlap_sphere(glm::vec3(0.0f), 1.0f).empty());

    engine_physics_query_filter_t filter{};
    filter.hit_triggers = true;
    EXPECT_EQ(overlap_sphere(glm::vec3(0.0f), 1.0f, &filter), std::vector<engine_game_object_t>{ trigger });
}

TEST_F(PhysicsTest, OverlapSphereSkipsIgnoredAndDisabledColliders)
{
    load_scene<PhysicsScene>();
    const auto ignored = create_object(0.5f, 0.0f, 0.0f);
    add_box_collider(ignored);
    const auto disabled = create_object(-0.5f, 0.0f, 0.0f);
    add_box_collider(disabled);
    set_collider_disabled(disabled, true);
    const auto kept = create_object(0.0f, 0.0f, 0.5f);
    add_box_collider(kept);
    step();

    engine_physics_query_filter_t filter{};
    filter.ignore = &ignored;
    filter.ignore_count = 1;
    EXPECT_EQ(overlap_sphere(glm::vec3(0.0f), 1.0f, &filter), std::vector<engine_game_object_t>{ kept });
}

TEST_F(PhysicsTest, OverlapSphereSeesAColliderCreatedInTheSameFrame)
{
    load_scene<PhysicsScene>();
    step();
    const auto go = create_object(2.0f, 0.0f, 0.0f);
    add_box_collider(go);

    EXPECT_EQ(overlap_sphere(glm::vec3(2.0f, 0.0f, 0.0f), 0.5f), std::vector<engine_game_object_t>{ go });
}

TEST_F(PhysicsTest, OverlapSphereSeesAColliderMovedInTheSameFrame)
{
    load_scene<PhysicsScene>();
    const auto go = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(go);
    set_body_type(go, ENGINE_RIGID_BODY_TYPE_KINEMATIC);
    step();

    move_to(go, 3.0f, 0.0f);

    EXPECT_EQ(overlap_sphere(glm::vec3(3.0f, 0.0f, 0.0f), 0.5f), std::vector<engine_game_object_t>{ go });
    EXPECT_TRUE(overlap_sphere(glm::vec3(0.0f), 0.5f).empty());
}

TEST_F(PhysicsTest, OverlapSphereSeesAChildWhereItsParentWasMovedInTheSameFrame)
{
    load_scene<PhysicsScene>();
    const auto parent = create_object(0.0f, 0.0f, 0.0f);
    const auto child = create_object(1.0f, 0.0f, 0.0f);
    set_parent(child, parent);
    add_box_collider(child);
    step();

    move_to(parent, 0.0f, 4.0f);

    EXPECT_EQ(overlap_sphere(glm::vec3(1.0f, 0.0f, 4.0f), 0.5f), std::vector<engine_game_object_t>{ child });
    EXPECT_TRUE(overlap_sphere(glm::vec3(1.0f, 0.0f, 0.0f), 0.5f).empty());
}

TEST_F(PhysicsTest, OverlapSphereReturnsNearestFirst)
{
    load_scene<PhysicsScene>();
    // their surfaces are 0.65, 0.25 and 0.55 from the center
    const auto far = create_object(0.0f, 0.0f, 0.9f);
    add_box_collider(far);
    const auto nearest = create_object(0.5f, 0.0f, 0.0f);
    add_box_collider(nearest);
    const auto middle = create_object(-0.8f, 0.0f, 0.0f);
    add_box_collider(middle);
    step();

    EXPECT_EQ(overlap_sphere(glm::vec3(0.0f), 1.0f), (std::vector<engine_game_object_t>{ nearest, middle, far }));
}

TEST_F(PhysicsTest, OverlapSphereKeepsTheNearestWhenTheBufferIsFull)
{
    load_scene<PhysicsScene>();
    const auto far = create_object(0.0f, 0.0f, 0.9f);
    add_box_collider(far);
    const auto nearest = create_object(0.5f, 0.0f, 0.0f);
    add_box_collider(nearest);
    step();

    const float center[3] = { 0.0f, 0.0f, 0.0f };
    engine_game_object_t one = ENGINE_INVALID_GAME_OBJECT_ID;
    EXPECT_EQ(enginePhysicsOverlapSphere(center, 1.0f, nullptr, &one, 1), 2u);
    EXPECT_EQ(one, nearest);
    // without a buffer it only counts
    EXPECT_EQ(enginePhysicsOverlapSphere(center, 1.0f, nullptr, nullptr, 0), 2u);
}

TEST_F(PhysicsTest, OverlapSphereWithZeroRadiusFindsNothing)
{
    load_scene<PhysicsScene>();
    const auto go = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(go);
    step();

    const float center[3] = { 0.0f, 0.0f, 0.0f };
    engine_game_object_t found[4]{};
    EXPECT_EQ(enginePhysicsOverlapSphere(center, 0.0f, nullptr, found, 4), 0u);
    EXPECT_EQ(enginePhysicsOverlapSphere(center, -1.0f, nullptr, found, 4), 0u);
    EXPECT_EQ(enginePhysicsOverlapSphere(nullptr, 1.0f, nullptr, found, 4), 0u);
    EXPECT_EQ(found[0], ENGINE_INVALID_GAME_OBJECT_ID);
}
}  // namespace

// collision layers (physics_layers.h): 1 level geometry, 2 characters, 3 hitboxes in these tests
namespace
{
constexpr std::uint32_t LEVEL_LAYER = 1;
constexpr std::uint32_t CHARACTER_LAYER = 2;
constexpr std::uint32_t HITBOX_LAYER = 3;

void set_layer(engine_game_object_t go, std::uint32_t layer, std::uint32_t collides_with = 0xFFFFFFFFu)
{
    auto cc = engineGameObjectGetColliderComponent(go);
    cc.layer = layer;
    cc.collides_with = collides_with;
    engineGameObjectUpdateColliderComponent(go, &cc);
}

// a dynamic 0.5 m box at (x, y, 0)
engine_game_object_t create_falling_box(float x, float y)
{
    const auto go = create_object(x, y, 0.0f);
    add_rigid_body(go, 1.0f);
    add_box_collider(go);
    return go;
}

// a floor whose top is at y = -0.25, on the level geometry layer
engine_game_object_t create_level_floor()
{
    const auto floor = create_object(0.0f, -0.5f, 0.0f);
    add_box_collider(floor, glm::vec3(5.0f, 0.25f, 5.0f));
    set_layer(floor, LEVEL_LAYER);
    return floor;
}

// moves the parent of a trigger 1 m to its right toward `target`; whether they collided on the way
bool sweep_hitbox_into(engine_game_object_t parent, engine_game_object_t hitbox, engine_game_object_t target, std::function<void()> step_frame)
{
    for (int i = 1; i <= 30; i++)
    {
        move_to(parent, i * 0.1f, 0.0f);
        step_frame();
        if (collided(hitbox, target))
        {
            return true;
        }
    }
    return false;
}
}  // namespace

TEST_F(PhysicsTest, NewCollidersAreOnTheDefaultLayerCollidingWithEverything)
{
    load_scene<PhysicsScene>();
    const auto go = create_object(0.0f, 0.0f, 0.0f);
    add_box_collider(go);
    const auto cc = engineGameObjectGetColliderComponent(go);
    EXPECT_EQ(cc.layer, static_cast<std::uint32_t>(ENGINE_PHYSICS_LAYER_DEFAULT));
    EXPECT_EQ(cc.collides_with, 0xFFFFFFFFu);
}

// a ray seeing only level geometry passes a character standing in front of the wall
TEST_F(PhysicsTest, RayCastOnALayerPassesBodiesOnOthers)
{
    load_scene<PhysicsScene>();
    const auto character = create_object(-2.0f, 0.0f, 0.0f);
    add_box_collider(character);
    set_layer(character, CHARACTER_LAYER);
    const auto wall = create_object(2.0f, 0.0f, 0.0f);
    add_box_collider(wall);
    set_layer(wall, LEVEL_LAYER);
    step();

    engine_ray_hit_t hit{};
    ASSERT_TRUE(cast_along_x(20.0f, nullptr, hit));
    EXPECT_EQ(hit.go, character);
    engine_physics_query_filter_t level_only{};
    level_only.layer_mask = 1u << LEVEL_LAYER;
    ASSERT_TRUE(cast_along_x(20.0f, &level_only, hit));
    EXPECT_EQ(hit.go, wall);
    // a mask with neither layer: nothing
    engine_physics_query_filter_t hitboxes_only{};
    hitboxes_only.layer_mask = 1u << HITBOX_LAYER;
    EXPECT_FALSE(cast_along_x(20.0f, &hitboxes_only, hit));
}

// an overlap seeing only characters skips the floor under them
TEST_F(PhysicsTest, OverlapOnALayerReturnsOnlyItsBodies)
{
    load_scene<PhysicsScene>();
    const auto floor = create_level_floor();
    const auto near_character = create_object(1.0f, 0.5f, 0.0f);
    add_box_collider(near_character);
    set_layer(near_character, CHARACTER_LAYER);
    const auto far_character = create_object(-2.0f, 0.5f, 0.0f);
    add_box_collider(far_character);
    set_layer(far_character, CHARACTER_LAYER);
    step();

    engine_physics_query_filter_t characters{};
    characters.layer_mask = 1u << CHARACTER_LAYER;
    EXPECT_EQ(overlap_sphere(glm::vec3(0.0f), 3.0f, &characters), (std::vector<engine_game_object_t>{ near_character, far_character }));
    // without a layer filter the floor is found too, nearest
    const auto all = overlap_sphere(glm::vec3(0.0f), 3.0f);
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all.front(), floor);
}

// two dynamic bodies whose masks exclude each other fall through each other; the floor stops both
TEST_F(PhysicsTest, BodiesOnExcludedLayersPassThroughEachOther)
{
    load_scene<PhysicsScene>();
    create_level_floor();
    const auto lower = create_falling_box(0.0f, 0.5f);
    const auto upper = create_falling_box(0.0f, 2.0f);
    // the upper one collides with the level only: it passes the lower one (on layer 0)
    set_layer(upper, CHARACTER_LAYER, 1u << LEVEL_LAYER);
    step(120);

    EXPECT_NEAR(world_position(lower).y, 0.0f, 0.05f);
    EXPECT_NEAR(world_position(upper).y, 0.0f, 0.05f);
}

// a mask of 0 collides with nothing: the body falls through the floor (queries still see it)
TEST_F(PhysicsTest, ColliderWithAnEmptyMaskCollidesWithNothing)
{
    load_scene<PhysicsScene>();
    create_level_floor();
    const auto box = create_falling_box(0.0f, 0.5f);
    set_layer(box, ENGINE_PHYSICS_LAYER_DEFAULT, 0);
    step(60);

    EXPECT_LT(world_position(box).y, -2.0f);
    EXPECT_TRUE(contains(overlap_sphere(world_position(box), 0.1f), box));
}

// a hitbox colliding only with characters never reports a kinematic body on another layer, and does on the characters'
TEST_F(PhysicsTest, HitboxOnlyDetectsTheLayersInItsMask)
{
    load_scene<PhysicsScene>();
    const auto target = create_object(4.0f, 0.0f, 0.0f);
    add_box_collider(target);
    set_body_type(target, ENGINE_RIGID_BODY_TYPE_KINEMATIC);
    set_layer(target, LEVEL_LAYER);
    const auto parent = create_object(0.0f, 0.0f, 0.0f);
    const auto hitbox = create_object(1.0f, 0.0f, 0.0f);
    set_parent(hitbox, parent);
    add_box_collider(hitbox);
    make_trigger(hitbox);
    set_layer(hitbox, HITBOX_LAYER, 1u << CHARACTER_LAYER);
    step();
    EXPECT_FALSE(sweep_hitbox_into(parent, hitbox, target, [this] { step(); }));

    move_to(parent, 0.0f, 0.0f);
    set_layer(target, CHARACTER_LAYER);
    step();
    EXPECT_TRUE(sweep_hitbox_into(parent, hitbox, target, [this] { step(); }));
}

// a layer changed while playing takes effect at once (the body is added again)
TEST_F(PhysicsTest, LayerChangedAtRuntimeTakesEffect)
{
    load_scene<PhysicsScene>();
    create_level_floor();
    const auto box = create_falling_box(0.0f, 0.5f);
    step(60);
    ASSERT_NEAR(world_position(box).y, 0.0f, 0.05f);

    // colliding with characters only: it falls through the floor
    set_layer(box, CHARACTER_LAYER, 1u << CHARACTER_LAYER);
    step(60);
    EXPECT_LT(world_position(box).y, -1.0f);
}
