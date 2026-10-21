#include "world_transform.h"
#include "engine.h"
#include "math_helpers.h"
#include "profiler.h"

#include <glm/gtx/matrix_decompose.hpp>

#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

glm::mat4 engine::compute_local_matrix(const engine_transform_component_t& transform)
{
    return compute_model_matrix(glm::make_vec3(transform.position), glm::make_quat(transform.rotation), glm::make_vec3(transform.scale));
}

void engine::update_world_transforms(entt::registry& registry, const JointMatrixFunction& joint_matrix)
{
    ENGINE_PROFILE_SECTION;
    {
        ENGINE_PROFILE_SECTION_N("transform_view");
        auto transform_view = registry.view<const engine_transform_component_t, world_transform_internal_component_t>();
        transform_view.each([](const engine_transform_component_t& transform, world_transform_internal_component_t& world)
            {
                world.local_to_world = compute_local_matrix(transform);
            });
    }

    {
        ENGINE_PROFILE_SECTION_N("joint_attachment_view");
        auto joint_attachment_view = registry.view<world_transform_internal_component_t, const engine_joint_attachment_component_t>();
        joint_attachment_view.each([&joint_matrix](world_transform_internal_component_t& world, const engine_joint_attachment_component_t& joint_attachment)
            {
                if (const auto joint = joint_matrix(joint_attachment))
                {
                    world.local_to_world = *joint * world.local_to_world;
                }
            });
    }

    {
        ENGINE_PROFILE_SECTION_N("parent_to_child_transform_view");
        // the chains read the parents' matrices before any of them is replaced by its world matrix
        std::vector<std::pair<entt::entity, glm::mat4>> world_matrices;
        auto parent_to_child_view = registry.view<const world_transform_internal_component_t, const engine_parent_component_t>();
        parent_to_child_view.each([&registry, &world_matrices](auto entity, const world_transform_internal_component_t& world, const engine_parent_component_t& parent_component)
            {
                auto local_to_world = world.local_to_world;
                auto parent = parent_component.parent;
                while (parent != ENGINE_INVALID_GAME_OBJECT_ID)
                {
                    const auto parent_entity = static_cast<entt::entity>(parent);
                    const auto* parent_world = registry.try_get<world_transform_internal_component_t>(parent_entity);
                    if (!parent_world)
                    {
                        break;
                    }
                    local_to_world = parent_world->local_to_world * local_to_world;
                    const auto* grandparent = registry.try_get<engine_parent_component_t>(parent_entity);
                    parent = grandparent ? grandparent->parent : ENGINE_INVALID_GAME_OBJECT_ID;
                }
                world_matrices.emplace_back(entity, local_to_world);
            });
        for (const auto& [entity, local_to_world] : world_matrices)
        {
            registry.get<world_transform_internal_component_t>(entity).local_to_world = local_to_world;
        }
    }
}

glm::mat4 engine::compute_world_matrix(const entt::registry& registry, entt::entity entity, const JointMatrixFunction& joint_matrix)
{
    const auto* transform = registry.try_get<engine_transform_component_t>(entity);
    if (!transform)
    {
        return glm::mat4{ 1.0f };
    }
    return get_parent_space_matrix(registry, entity, joint_matrix) * compute_local_matrix(*transform);
}

glm::mat4 engine::get_parent_space_matrix(const entt::registry& registry, entt::entity entity, const JointMatrixFunction& joint_matrix)
{
    glm::mat4 parent_space{ 1.0f };
    if (const auto* parent = registry.try_get<engine_parent_component_t>(entity); parent && parent->parent != ENGINE_INVALID_GAME_OBJECT_ID)
    {
        // like update_world_transforms: a parent without a transform ends the chain
        const auto parent_entity = static_cast<entt::entity>(parent->parent);
        if (registry.all_of<engine_transform_component_t>(parent_entity))
        {
            parent_space = compute_world_matrix(registry, parent_entity, joint_matrix);
        }
    }
    if (const auto* joint_attachment = registry.try_get<engine_joint_attachment_component_t>(entity))
    {
        if (const auto joint = joint_matrix(*joint_attachment))
        {
            parent_space = parent_space * *joint;
        }
    }
    return parent_space;
}

bool engine::local_transform_from_world(const glm::mat4& parent_space, const glm::mat4& world, engine_transform_component_t& out)
{
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    // a zero scale (in the parent space or the result) has no inverse and no rotation: decompose would divide by zero.
    // It leaves its outputs uninitialized when it fails.
    constexpr float min_determinant = 1e-12f;
    if (std::abs(glm::determinant(parent_space)) < min_determinant)
    {
        return false;
    }
    const auto local = glm::inverse(parent_space) * world;
    if (std::abs(glm::determinant(local)) < min_determinant
        || !glm::decompose(local, scale, rotation, translation, skew, perspective))
    {
        return false;
    }
    rotation = glm::normalize(rotation);
    std::memcpy(out.position, glm::value_ptr(translation), sizeof(out.position));
    std::memcpy(out.rotation, glm::value_ptr(rotation), sizeof(out.rotation));
    std::memcpy(out.scale, glm::value_ptr(scale), sizeof(out.scale));
    return true;
}
