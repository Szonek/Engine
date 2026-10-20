#pragma once
#include "components/transform_component.h"
#include "components/skin_component.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <functional>
#include <optional>

namespace engine
{
// The world matrix of a game object with a transform component: derived from its transform (local to the parent), its
// joint attachment and its parents. Added and removed with the transform component, written only by
// update_world_transforms, never saved. Valid after the last scene update.
struct world_transform_internal_component_t
{
    glm::mat4 local_to_world{ 1.0f };
};

// The model matrix of the attachment's joint (in the space of the skin's game object), or nullopt when there is none.
using JointMatrixFunction = std::function<std::optional<glm::mat4>(const engine_joint_attachment_component_t&)>;

glm::mat4 compute_local_matrix(const engine_transform_component_t& transform);

// local_to_world of every transform: its TRS, then its joint attachment, then the parent chain.
// Writes only world_transform_internal_component_t, so no update listeners or observers fire.
void update_world_transforms(entt::registry& registry, const JointMatrixFunction& joint_matrix);

// The space the entity's transform is local to: its parent's world matrix times its joint attachment's joint matrix
// (each identity when there is none), so local_to_world = parent space * local matrix.
glm::mat4 get_parent_space_matrix(const entt::registry& registry, entt::entity entity, const JointMatrixFunction& joint_matrix);

// The local transform (position, rotation, scale) which puts a game object whose parent space is `parent_space` at `world`.
// Returns false and leaves `out` untouched when the matrix can't be decomposed (e.g. a zero scale). Shear (a rotated
// child under a non-uniformly scaled parent) can't be represented by TRS: the result is then the closest TRS.
bool local_transform_from_world(const glm::mat4& parent_space, const glm::mat4& world, engine_transform_component_t& out);
}  // namespace engine
