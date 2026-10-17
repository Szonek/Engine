#pragma once
#include <entt/entt.hpp>

#include <cstdint>
#include <unordered_map>

namespace engine
{
// Persistent identity of an entity, unlike entt::entity it is meant to be written to files. 0 is never a valid ID.
struct entity_id_component_t
{
    std::uint64_t id;
};

// Hands out persistent entity IDs and maps them back to entities. IDs come from a counter that starts at 1 and are
// never reused, so building the same scene in the same order gives the same IDs in every run.
// The entity_id_component_t of an entity is owned by this class: it is added here and released when it is destroyed.
class EntityIdRegistry
{
public:
    static constexpr std::uint64_t INVALID_ID = 0;

    explicit EntityIdRegistry(entt::registry& registry)
        : registry_(registry)
    {
        registry_.on_destroy<entity_id_component_t>().connect<&EntityIdRegistry::on_id_destroyed>(*this);
    }
    EntityIdRegistry(const EntityIdRegistry&) = delete;
    EntityIdRegistry(EntityIdRegistry&&) = delete;
    EntityIdRegistry& operator=(const EntityIdRegistry&) = delete;
    EntityIdRegistry& operator=(EntityIdRegistry&&) = delete;
    ~EntityIdRegistry()
    {
        registry_.on_destroy<entity_id_component_t>().disconnect<&EntityIdRegistry::on_id_destroyed>(*this);
    }

    // Gives the entity the next free ID.
    std::uint64_t assign_next(entt::entity entity)
    {
        while (entities_.contains(next_id_))
        {
            next_id_++;
        }
        const auto id = next_id_++;
        assign(entity, id);
        return id;
    }

    // Gives the entity a known ID (e.g. read from a file, or restored by undo). Fails when the ID is invalid or
    // already used, or the entity already has an ID. Later assign_next() calls never return this ID.
    bool assign(entt::entity entity, std::uint64_t id)
    {
        if (id == INVALID_ID || entities_.contains(id) || !registry_.valid(entity) || registry_.all_of<entity_id_component_t>(entity))
        {
            return false;
        }
        registry_.emplace<entity_id_component_t>(entity, id);
        entities_[id] = entity;
        if (id >= next_id_)
        {
            next_id_ = id + 1;
        }
        return true;
    }

    // INVALID_ID when the entity has no ID.
    std::uint64_t get_id(entt::entity entity) const
    {
        if (!registry_.valid(entity))
        {
            return INVALID_ID;
        }
        const auto* comp = registry_.try_get<entity_id_component_t>(entity);
        return comp ? comp->id : INVALID_ID;
    }

    // entt::null when no entity has this ID.
    entt::entity find(std::uint64_t id) const
    {
        const auto it = entities_.find(id);
        return it == entities_.end() ? entt::entity{ entt::null } : it->second;
    }

private:
    void on_id_destroyed(entt::registry& registry, entt::entity entity)
    {
        entities_.erase(registry.get<entity_id_component_t>(entity).id);
    }

private:
    entt::registry& registry_;
    std::unordered_map<std::uint64_t, entt::entity> entities_;
    std::uint64_t next_id_ = 1;
};
}  // namespace engine
