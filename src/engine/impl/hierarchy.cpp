#include "hierarchy.h"
#include "logger.h"
#include "components_internals/children_internal_component.h"

#include <engine.h>

#include <fmt/format.h>

#include <algorithm>
#include <cstdint>

namespace
{
void unlist(entt::registry& registry, entt::entity entity, std::uint32_t parent)
{
    if (auto* children = registry.try_get<engine::children_internal_component_t>(static_cast<entt::entity>(parent)))
    {
        std::erase(children->children, entity);
    }
}

void change_parent_component(entt::registry& registry, entt::entity entity, const engine_parent_component_t& previous)
{
    const auto& parent = registry.get<engine_parent_component_t>(entity);
    // a change which keeps the parent keeps the child's place among its siblings
    if (parent.parent == previous.parent && parent.parent != ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return;
    }
    unlist(registry, entity, previous.parent);
    if (parent.parent == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        engine::log::log(engine::log::LogLevel::eCritical, fmt::format("Parent component has invalid parent id. If entity doesnt have parent than just delete it. Are you sure you are doing valid thing?\n"));
        return;
    }
    registry.get_or_emplace<engine::children_internal_component_t>(static_cast<entt::entity>(parent.parent)).children.push_back(entity);
}

void destroy_parent_component(entt::registry& registry, entt::entity entity)
{
    unlist(registry, entity, registry.get<engine_parent_component_t>(entity).parent);
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
    registry.storage<engine_parent_component_t>().on_change().connect<&change_parent_component>(registry);
    registry.on_destroy<engine_parent_component_t>().connect<&destroy_parent_component>();
    registry.on_destroy<children_internal_component_t>().connect<&destroy_children_component>();
}
