#include "scenario_harness.h"

#include <iscene.h>

#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

// The engine's kinematic character controller: a capsule moved with engineCharacterControllerMove which falls with
// gravity, slides along walls, walks up steps and pushes dynamic bodies.
namespace
{
class CharacterScene : public engine::IScene
{
public:
    CharacterScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "scenario_character_controller"; }
};

constexpr float RADIUS = 0.3f;
constexpr float HEIGHT = 1.8f;

engine_game_object_t create_object(const glm::vec3& position)
{
    const auto go = engineGameObjectCreate();
    auto tc = engineGameObjectAddTransformComponent(go);
    tc.position[0] = position.x;
    tc.position[1] = position.y;
    tc.position[2] = position.z;
    engineGameObjectUpdateTransformComponent(go, &tc);
    return go;
}

// a static box from `min` to `max`
engine_game_object_t create_box(const glm::vec3& min, const glm::vec3& max)
{
    const auto go = create_object((min + max) * 0.5f);
    auto cc = engineGameObjectAddColliderComponent(go);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    const auto half_extents = (max - min) * 0.5f;
    cc.collider.box.size[0] = half_extents.x;
    cc.collider.box.size[1] = half_extents.y;
    cc.collider.box.size[2] = half_extents.z;
    engineGameObjectUpdateColliderComponent(go, &cc);
    return go;
}

// a ramp rising along +x from (1, 0, 0) at `degrees`
engine_game_object_t create_ramp(float degrees)
{
    constexpr float half_length = 3.0f;
    constexpr float half_thickness = 0.1f;
    const float angle = glm::radians(degrees);
    const auto go = create_object({ 1.0f + half_length * std::cos(angle) + half_thickness * std::sin(angle),
        half_length * std::sin(angle) - half_thickness * std::cos(angle), 0.0f });
    auto tc = engineGameObjectGetTransformComponent(go);
    const auto rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
    tc.rotation[0] = rotation.x;
    tc.rotation[1] = rotation.y;
    tc.rotation[2] = rotation.z;
    tc.rotation[3] = rotation.w;
    engineGameObjectUpdateTransformComponent(go, &tc);
    auto cc = engineGameObjectAddColliderComponent(go);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    cc.collider.box.size[0] = half_length;
    cc.collider.box.size[1] = half_thickness;
    cc.collider.box.size[2] = 5.0f;
    engineGameObjectUpdateColliderComponent(go, &cc);
    return go;
}

engine_game_object_t create_character(const glm::vec3& feet)
{
    const auto go = create_object(feet);
    auto controller = engineGameObjectAddCharacterControllerComponent(go);
    controller.radius = RADIUS;
    controller.height = HEIGHT;
    controller.step_height = 0.3f;
    engineGameObjectUpdateCharacterControllerComponent(go, &controller);
    return go;
}

glm::vec3 position_of(engine_game_object_t go)
{
    const auto tc = engineGameObjectGetTransformComponent(go);
    return { tc.position[0], tc.position[1], tc.position[2] };
}

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

class CharacterControllerTest : public scenario::ScenarioTest
{
protected:
    void SetUp() override
    {
        load_scene<CharacterScene>();
        // the floor's top is at y = 0
        create_box({ -10.0f, -1.0f, -10.0f }, { 10.0f, 0.0f, 10.0f });
    }

    // moves the character with `velocity` (m/s) for `frames` frames, the way a script does: once per frame
    engine_character_controller_move_result_t walk(engine_game_object_t character, const glm::vec3& velocity, std::uint32_t frames)
    {
        engine_character_controller_move_result_t result{};
        for (std::uint32_t i = 0; i < frames; i++)
        {
            EXPECT_EQ(engineCharacterControllerMove(character, &velocity.x, scenario::FIXED_DT_MS, &result), ENGINE_RESULT_CODE_OK);
            step();
        }
        return result;
    }
};
}  // namespace

TEST_F(CharacterControllerTest, FallsAndLandsOnTheFloor)
{
    const auto character = create_character({ 0.0f, 2.0f, 0.0f });

    const auto result = walk(character, glm::vec3(0.0f), 90);

    EXPECT_TRUE(result.grounded);
    EXPECT_NEAR(position_of(character).y, 0.0f, 0.02f);
}

TEST_F(CharacterControllerTest, StandsStillOnTheFloor)
{
    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    walk(character, glm::vec3(0.0f), 30);

    const auto result = walk(character, glm::vec3(0.0f), 120);

    EXPECT_TRUE(result.grounded);
    EXPECT_NEAR(position_of(character).y, 0.0f, 0.02f);
    EXPECT_NEAR(position_of(character).x, 0.0f, 1e-4f);
}

TEST_F(CharacterControllerTest, WalksAtItsVelocity)
{
    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    walk(character, glm::vec3(0.0f), 10);

    walk(character, { 2.0f, 0.0f, 0.0f }, 60);

    EXPECT_NEAR(position_of(character).x, 2.0f, 0.02f);
}

TEST_F(CharacterControllerTest, SlidesAlongAWallInsteadOfStopping)
{
    // a wall whose surface is at x = 1
    create_box({ 1.0f, 0.0f, -10.0f }, { 1.5f, 3.0f, 10.0f });
    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    walk(character, glm::vec3(0.0f), 10);

    float max_x = std::numeric_limits<float>::lowest();
    for (int i = 0; i < 60; i++)
    {
        const auto result = walk(character, { 2.0f, 0.0f, -2.0f }, 1);
        max_x = std::max(max_x, position_of(character).x);
        EXPECT_TRUE(result.grounded);
    }

    EXPECT_LT(max_x, 1.0f - RADIUS + 0.01f);
    EXPECT_GT(max_x, 1.0f - RADIUS - 0.05f);
    // the part of the velocity along the wall is kept
    EXPECT_LT(position_of(character).z, -1.8f);
}

TEST_F(CharacterControllerTest, WalksUpALowStepButNotOntoAHighOne)
{
    // a 0.2 m step from x = 1 and a 0.6 m ledge from x = 3
    create_box({ 1.0f, 0.0f, -10.0f }, { 10.0f, 0.2f, 10.0f });
    create_box({ 3.0f, 0.0f, -10.0f }, { 10.0f, 0.6f, 10.0f });
    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    walk(character, glm::vec3(0.0f), 10);

    walk(character, { 2.0f, 0.0f, 0.0f }, 45);
    EXPECT_NEAR(position_of(character).y, 0.2f, 0.02f);
    EXPECT_GT(position_of(character).x, 1.0f);

    const auto result = walk(character, { 2.0f, 0.0f, 0.0f }, 60);
    EXPECT_LT(position_of(character).x, 3.0f - RADIUS + 0.01f);
    EXPECT_NEAR(position_of(character).y, 0.2f, 0.02f);
    EXPECT_TRUE(result.hit_wall);
}

TEST_F(CharacterControllerTest, SticksToTheGroundWalkingDownAStep)
{
    create_box({ -10.0f, 0.0f, -10.0f }, { 1.0f, 0.2f, 10.0f });
    const auto character = create_character({ 0.0f, 0.2f, 0.0f });
    walk(character, glm::vec3(0.0f), 10);

    // off the step: snapped down instead of falling, also while its round bottom rolls over the edge
    for (int i = 0; i < 40; i++)
    {
        EXPECT_TRUE(walk(character, { 2.0f, 0.0f, 0.0f }, 1).grounded);
    }
    EXPECT_NEAR(position_of(character).y, 0.0f, 0.02f);
}

TEST_F(CharacterControllerTest, WalksUpAWalkableRamp)
{
    create_ramp(20.0f);
    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    walk(character, glm::vec3(0.0f), 10);

    const auto result = walk(character, { 2.0f, 0.0f, 0.0f }, 60);

    EXPECT_TRUE(result.grounded);
    EXPECT_GT(position_of(character).x, 1.9f);
    EXPECT_NEAR(position_of(character).y, (position_of(character).x - 1.0f) * std::tan(glm::radians(20.0f)), 0.1f);
}

TEST_F(CharacterControllerTest, IsBlockedByATooSteepSlope)
{
    create_ramp(60.0f);
    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    walk(character, glm::vec3(0.0f), 10);

    const auto result = walk(character, { 2.0f, 0.0f, 0.0f }, 60);

    EXPECT_LT(position_of(character).y, 0.35f);
    EXPECT_LT(position_of(character).x, 1.2f);
    EXPECT_TRUE(result.hit_wall);
}

TEST_F(CharacterControllerTest, IsBlockedByAnotherCharacter)
{
    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    const auto other = create_character({ 1.5f, 0.0f, 0.0f });
    walk(character, glm::vec3(0.0f), 10);

    walk(character, { 2.0f, 0.0f, 0.0f }, 60);

    EXPECT_LT(position_of(character).x, 1.5f - 2.0f * RADIUS + 0.01f);
    EXPECT_NEAR(position_of(other).x, 1.5f, 1e-4f);
}

TEST_F(CharacterControllerTest, WalksThroughTriggersAndIsSeenByThem)
{
    // a hitbox: a trigger following a parent (kinematic)
    const auto parent = create_object({ 1.0f, 0.0f, 0.0f });
    const auto trigger = create_object({ 0.0f, 0.5f, 0.0f });
    auto pc = engineGameObjectAddParentComponent(trigger);
    pc.parent = parent;
    engineGameObjectUpdateParentComponent(trigger, &pc);
    auto cc = engineGameObjectAddColliderComponent(trigger);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    cc.is_trigger = true;
    cc.collider.box.size[0] = 0.2f;
    cc.collider.box.size[1] = 0.5f;
    cc.collider.box.size[2] = 1.0f;
    engineGameObjectUpdateColliderComponent(trigger, &cc);
    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    walk(character, glm::vec3(0.0f), 10);

    bool seen = false;
    for (int i = 0; i < 60; i++)
    {
        walk(character, { 2.0f, 0.0f, 0.0f }, 1);
        seen = seen || collided(character, trigger);
    }

    EXPECT_TRUE(seen);
    EXPECT_GT(position_of(character).x, 1.9f);
}

TEST_F(CharacterControllerTest, PushesADynamicBody)
{
    const auto box = create_box({ 0.8f, 0.0f, -0.2f }, { 1.2f, 0.4f, 0.2f });
    auto rb = engineGameObjectAddRigidBodyComponent(box);
    rb.mass = 1.0f;
    engineGameObjectUpdateRigidBodyComponent(box, &rb);
    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    walk(character, glm::vec3(0.0f), 10);

    walk(character, { 2.0f, 0.0f, 0.0f }, 45);

    // dynamic bodies don't block the character: they are pushed out of its way
    EXPECT_GT(position_of(character).x, 1.4f);
    EXPECT_GT(std::abs(position_of(box).x - 1.0f) + std::abs(position_of(box).z), 0.2f);
}

TEST_F(CharacterControllerTest, MoveFailsWithoutTheComponentOrWithAParent)
{
    const auto plain = create_object({ 0.0f, 0.0f, 0.0f });
    const glm::vec3 velocity(1.0f, 0.0f, 0.0f);
    EXPECT_EQ(engineCharacterControllerMove(plain, &velocity.x, scenario::FIXED_DT_MS, nullptr), ENGINE_RESULT_CODE_FAIL);

    const auto character = create_character({ 0.0f, 0.0f, 0.0f });
    auto pc = engineGameObjectAddParentComponent(character);
    pc.parent = plain;
    engineGameObjectUpdateParentComponent(character, &pc);
    EXPECT_EQ(engineCharacterControllerMove(character, &velocity.x, scenario::FIXED_DT_MS, nullptr), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(engineCharacterControllerMove(character, nullptr, scenario::FIXED_DT_MS, nullptr), ENGINE_RESULT_CODE_INVALID_ARG);
}
