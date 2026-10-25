#include "world_transform.h"
#include "engine.h"

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstring>

namespace
{
engine_transform_component_t make_transform(const glm::vec3& position, const glm::quat& rotation = glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)), const glm::vec3& scale = glm::vec3(1.0f))
{
    engine_transform_component_t transform{};
    std::memcpy(transform.position, glm::value_ptr(position), sizeof(transform.position));
    std::memcpy(transform.rotation, glm::value_ptr(rotation), sizeof(transform.rotation));
    std::memcpy(transform.scale, glm::value_ptr(scale), sizeof(transform.scale));
    return transform;
}

// the same TRS as a matrix, built independently of the code under test
glm::mat4 trs_matrix(const glm::vec3& position, const glm::quat& rotation = glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)), const glm::vec3& scale = glm::vec3(1.0f))
{
    return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

void expect_matrix_near(const glm::mat4& actual, const glm::mat4& expected)
{
    for (int c = 0; c < 4; c++)
    {
        for (int r = 0; r < 4; r++)
        {
            EXPECT_TRUE(std::isfinite(actual[c][r]));
            EXPECT_NEAR(actual[c][r], expected[c][r], 1e-4f) << "column " << c << ", row " << r;
        }
    }
}

std::optional<glm::mat4> no_joints(const engine_joint_attachment_component_t&)
{
    return std::nullopt;
}

class WorldTransformTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // ENGINE_INVALID_GAME_OBJECT_ID is 0, so a game object with entity 0 can't be a parent: keep it out of the tests
        [[maybe_unused]] const auto entity_zero = registry_.create();
    }

    entt::entity create(const engine_transform_component_t& transform, entt::entity parent = entt::null)
    {
        const auto entity = registry_.create();
        registry_.emplace<engine_transform_component_t>(entity, transform);
        registry_.emplace<engine::world_transform_internal_component_t>(entity);
        if (parent != entt::null)
        {
            registry_.emplace<engine_parent_component_t>(entity, engine_parent_component_t{ static_cast<engine_game_object_t>(parent) });
        }
        return entity;
    }

    const glm::mat4& world(entt::entity entity) const
    {
        return registry_.get<engine::world_transform_internal_component_t>(entity).local_to_world;
    }

    entt::registry registry_;
};
}  // namespace

TEST_F(WorldTransformTest, WithoutParentWorldIsTheLocalTransform)
{
    const auto rotation = glm::angleAxis(glm::radians(30.0f), glm::normalize(glm::vec3(1.0f, 2.0f, 0.5f)));
    const auto entity = create(make_transform({ 1.0f, 2.0f, 3.0f }, rotation, { 2.0f, 1.0f, 0.5f }));

    engine::update_world_transforms(registry_, no_joints);

    expect_matrix_near(world(entity), trs_matrix({ 1.0f, 2.0f, 3.0f }, rotation, { 2.0f, 1.0f, 0.5f }));
}

TEST_F(WorldTransformTest, ThreeLevelChainMultipliesTheParents)
{
    const auto root_rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const auto grandchild_rotation = glm::angleAxis(glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    const auto root = create(make_transform({ 1.0f, 2.0f, 3.0f }, root_rotation, glm::vec3(2.0f)));
    const auto child = create(make_transform({ 1.0f, 0.0f, 0.0f }, glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)), { 1.0f, 3.0f, 1.0f }), root);
    const auto grandchild = create(make_transform({ 0.0f, 1.0f, 0.0f }, grandchild_rotation), child);

    engine::update_world_transforms(registry_, no_joints);

    const auto root_world = trs_matrix({ 1.0f, 2.0f, 3.0f }, root_rotation, glm::vec3(2.0f));
    const auto child_world = root_world * trs_matrix({ 1.0f, 0.0f, 0.0f }, glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)), { 1.0f, 3.0f, 1.0f });
    expect_matrix_near(world(root), root_world);
    expect_matrix_near(world(child), child_world);
    expect_matrix_near(world(grandchild), child_world * trs_matrix({ 0.0f, 1.0f, 0.0f }, grandchild_rotation));
}

TEST_F(WorldTransformTest, RepeatedUpdatesDontAccumulate)
{
    const auto parent = create(make_transform({ 0.0f, 0.0f, 5.0f }));
    const auto child = create(make_transform({ 1.0f, 0.0f, 0.0f }), parent);

    engine::update_world_transforms(registry_, no_joints);
    engine::update_world_transforms(registry_, no_joints);

    expect_matrix_near(world(child), trs_matrix({ 1.0f, 0.0f, 5.0f }));
}

TEST_F(WorldTransformTest, JointAttachmentIsBetweenTheParentAndTheLocalTransform)
{
    const auto parent_rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const auto parent = create(make_transform({ 10.0f, 0.0f, 0.0f }, parent_rotation));
    const auto attached = create(make_transform({ 0.0f, 0.0f, 0.5f }), parent);
    registry_.emplace<engine_joint_attachment_component_t>(attached, engine_joint_attachment_component_t{ static_cast<engine_game_object_t>(parent), nullptr });

    const auto joint = trs_matrix({ 0.0f, 1.5f, 0.2f }, glm::angleAxis(glm::radians(20.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
    const auto joint_matrix = [&joint](const engine_joint_attachment_component_t&) -> std::optional<glm::mat4> { return joint; };
    engine::update_world_transforms(registry_, joint_matrix);

    const auto parent_space = trs_matrix({ 10.0f, 0.0f, 0.0f }, parent_rotation) * joint;
    expect_matrix_near(world(attached), parent_space * trs_matrix({ 0.0f, 0.0f, 0.5f }));
    expect_matrix_near(engine::get_parent_space_matrix(registry_, attached, joint_matrix), parent_space);
    expect_matrix_near(engine::get_parent_space_matrix(registry_, parent, joint_matrix), glm::mat4(1.0f));
}

// physics computes world matrices from the transforms between update_world_transforms passes
TEST_F(WorldTransformTest, ComputeWorldMatrixMatchesUpdateWorldTransformsWithoutIt)
{
    const auto root_rotation = glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const auto root = create(make_transform({ 1.0f, 2.0f, 3.0f }, root_rotation, glm::vec3(2.0f)));
    const auto child = create(make_transform({ 1.0f, 0.0f, 0.0f }), root);
    const auto attached = create(make_transform({ 0.0f, 0.5f, 0.0f }, glm::angleAxis(glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f))), child);
    registry_.emplace<engine_joint_attachment_component_t>(attached, engine_joint_attachment_component_t{ static_cast<engine_game_object_t>(root), nullptr });
    const auto joint = trs_matrix({ 0.0f, 1.0f, 0.2f }, glm::angleAxis(glm::radians(20.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
    const auto joint_matrix = [&joint](const engine_joint_attachment_component_t&) -> std::optional<glm::mat4> { return joint; };

    // the world transform components are still identity: nothing was computed yet
    const auto computed = engine::compute_world_matrix(registry_, attached, joint_matrix);
    engine::update_world_transforms(registry_, joint_matrix);

    expect_matrix_near(computed, world(attached));
    expect_matrix_near(engine::compute_world_matrix(registry_, child, joint_matrix), world(child));
    expect_matrix_near(engine::compute_world_matrix(registry_, root, joint_matrix), world(root));
}

// what the editor gizmo does: it moves the world matrix and writes the local transform back
TEST_F(WorldTransformTest, LocalTransformFromWorldPutsTheChildAtThatWorldMatrix)
{
    const auto parent_rotation = glm::angleAxis(glm::radians(60.0f), glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f)));
    const auto parent = create(make_transform({ 3.0f, -1.0f, 2.0f }, parent_rotation, { 2.0f, 0.5f, 1.0f }));
    const auto child = create(make_transform({ 1.0f, 1.0f, 0.0f }), parent);
    engine::update_world_transforms(registry_, no_joints);

    // dragged by a world offset, and scaled along the parent's axes (any matrix TRS can represent under that parent)
    const auto targets = {
        glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 2.0f, -1.0f)) * world(child),
        world(parent) * trs_matrix({ -2.0f, 0.0f, 4.0f }, glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)), { 3.0f, 1.0f, 0.25f }),
    };
    for (const auto& target : targets)
    {
        auto transform = registry_.get<engine_transform_component_t>(child);
        ASSERT_TRUE(engine::local_transform_from_world(engine::get_parent_space_matrix(registry_, child, no_joints), target, transform));
        registry_.replace<engine_transform_component_t>(child, transform);

        engine::update_world_transforms(registry_, no_joints);

        expect_matrix_near(world(child), target);
    }
}

TEST_F(WorldTransformTest, LocalTransformFromWorldKeepsARotationUnderARotatedParent)
{
    const auto parent_rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const auto parent = create(make_transform({ 0.0f, 0.0f, 0.0f }, parent_rotation, glm::vec3(2.0f)));
    const auto child = create(make_transform({ 0.0f, 0.0f, 0.0f }), parent);
    engine::update_world_transforms(registry_, no_joints);

    const auto target = trs_matrix({ 1.0f, 2.0f, 3.0f }, glm::angleAxis(glm::radians(-30.0f), glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(4.0f));
    auto transform = registry_.get<engine_transform_component_t>(child);
    ASSERT_TRUE(engine::local_transform_from_world(world(parent), target, transform));

    EXPECT_NEAR(transform.scale[0], 2.0f, 1e-4f);
    EXPECT_NEAR(transform.scale[1], 2.0f, 1e-4f);
    EXPECT_NEAR(transform.scale[2], 2.0f, 1e-4f);
    registry_.replace<engine_transform_component_t>(child, transform);
    engine::update_world_transforms(registry_, no_joints);
    expect_matrix_near(world(child), target);
}

// a move with the gizmo under a rotated, scaled parent: the decomposition rounds the rotation and scale it didn't change
TEST_F(WorldTransformTest, LocalTransformFromWorldWritesOnlyTheChangedParts)
{
    const auto parent_rotation = glm::angleAxis(glm::radians(37.0f), glm::normalize(glm::vec3(1.0f, 2.0f, 0.5f)));
    const auto parent = create(make_transform({ 3.0f, -1.0f, 2.0f }, parent_rotation, glm::vec3(0.35f)));
    const auto child_rotation = glm::angleAxis(glm::radians(-71.0f), glm::normalize(glm::vec3(0.2f, 1.0f, -0.4f)));
    const auto child = create(make_transform({ 1.0f, 1.0f, 0.0f }, child_rotation), parent);
    engine::update_world_transforms(registry_, no_joints);
    const auto before = registry_.get<engine_transform_component_t>(child);
    const auto target = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 2.0f, -1.0f)) * world(child);

    auto moved = before;
    ASSERT_TRUE(engine::local_transform_from_world(world(parent), target, moved, { .position = true, .rotation = false, .scale = false }));
    EXPECT_NE(std::memcmp(moved.position, before.position, sizeof(moved.position)), 0);
    EXPECT_EQ(std::memcmp(moved.rotation, before.rotation, sizeof(moved.rotation)), 0);
    EXPECT_EQ(std::memcmp(moved.scale, before.scale, sizeof(moved.scale)), 0);
    registry_.replace<engine_transform_component_t>(child, moved);
    engine::update_world_transforms(registry_, no_joints);
    expect_matrix_near(world(child), target);
}

TEST_F(WorldTransformTest, LocalTransformFromWorldFailsForAZeroScaleWithoutTouchingTheTransform)
{
    const auto before = make_transform({ 1.0f, 2.0f, 3.0f });
    auto transform = before;

    EXPECT_FALSE(engine::local_transform_from_world(glm::scale(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 1.0f)), glm::mat4(1.0f), transform));
    EXPECT_FALSE(engine::local_transform_from_world(glm::mat4(1.0f), glm::scale(glm::mat4(1.0f), glm::vec3(0.0f)), transform));

    EXPECT_EQ(std::memcmp(&transform, &before, sizeof(transform)), 0);
}
