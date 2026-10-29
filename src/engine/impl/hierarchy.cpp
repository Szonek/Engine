#include "hierarchy.h"
#include "logger.h"
#include "components_internals/children_internal_component.h"

#include <engine.h>

#include <fmt/format.h>

#include <algorithm>

namespace
{
// takes the game object out of the children list it is in, if any
void unlist(entt::registry& registry, entt::entity entity)
{
    auto* listed = registry.try_get<engine::listed_parent_internal_component_t>(entity);
    if (!listed || listed->parent == entt::null)
    {
        return;
    }
    if (auto* children = registry.try_get<engine::children_internal_component_t>(listed->parent))
    {
        std::erase(children->children, entity);
    }
    // not removed: this runs inside the destroy listeners of the game object's other components
    listed->parent = entt::null;
}

void update_parent_component(entt::registry& registry, entt::entity entity)
{
    const auto& parent = registry.get<engine_parent_component_t>(entity);
    const auto parent_entt = static_cast<entt::entity>(parent.parent);
    const auto& listed = registry.get_or_emplace<engine::listed_parent_internal_component_t>(entity);
    // an update which keeps the parent keeps the child's place among its siblings
    if (listed.parent == parent_entt && listed.parent != entt::null)
    {
        return;
    }
    unlist(registry, entity);
    if (parent.parent == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        engine::log::log(engine::log::LogLevel::eCritical, fmt::format("Parent component has invalid parent id. If entity doesnt have parent than just delete it. Are you sure you are doing valid thing?\n"));
        return;
    }
    registry.get_or_emplace<engine::children_internal_component_t>(parent_entt).children.push_back(entity);
    registry.get<engine::listed_parent_internal_component_t>(entity).parent = parent_entt;
}

void destroy_parent_component(entt::registry& registry, entt::entity entity)
{
    if (registry.all_of<engine::listed_parent_internal_component_t>(entity))
    {
        unlist(registry, entity);
        return;
    }
    // the game object is being destroyed and its listed parent went first: it is the one its parent component names,
    // which a listed game object always keeps in step
    const auto parent = static_cast<entt::entity>(registry.get<engine_parent_component_t>(entity).parent);
    if (auto* children = registry.try_get<engine::children_internal_component_t>(parent))
    {
        std::erase(children->children, entity);
    }
}

void destroy_children_component(entt::registry& registry, entt::entity entity)
{
    // a copy: removing a child's parent component removes the child from this list
    const auto children = registry.get<engine::children_internal_component_t>(entity).children;
    for (const auto child : children)
    {
        registry.remove<engine_parent_component_t>(child);
    }
}
}  // namespace

void engine::connect_hierarchy(entt::registry& registry)
{
    registry.on_update<engine_parent_component_t>().connect<&update_parent_component>();
    registry.on_destroy<engine_parent_component_t>().connect<&destroy_parent_component>();
    registry.on_destroy<children_internal_component_t>().connect<&destroy_children_component>();
}
