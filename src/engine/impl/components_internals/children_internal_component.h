#pragma once
#include <entt/entt.hpp>

#include <vector>

namespace engine
{
// The game objects whose parent component points at this game object, in the order they were parented. Derived from
// the parent components: kept up to date by the scene, never saved and not part of the API as a component.
struct children_internal_component_t
{
    std::vector<entt::entity> children;
};
}  // namespace engine
