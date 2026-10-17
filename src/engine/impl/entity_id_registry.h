#pragma once
#include <entt/entt.hpp>

#include <cstdint>
#include <limits>
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
    // the counter moves one past the highest ID in use, so the largest value can never be an ID
    static constexpr std::uint64_t MAX_ID = std::numeric_limits<std::uint64_t>::max() - 1;

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

    // Gives the entity the next free ID. INVALID_ID (and no ID) when the entity can't get one: it is not valid,
    // already has an ID, or the IDs are exhausted.
    std::uint64_t assign_next(entt::entity entity)
    {
        while (next_id_ <= MAX_ID && entities_.contains(next_id_))
        {
            next_id_++;
        }
        const auto id = next_id_;
        return assign(entity, id) ? id : INVALID_ID;
    }

    // Gives the entity a known ID (e.g. read from a file, or restored by undo). Fails when the ID is invalid,
    // above MAX_ID or already used, or the entity already has an ID. Later assign_next() calls never return this ID.
    bool assign(entt::entity entity, std::uint64_t id)
    {
        if (id == INVALID_ID || id > MAX_ID || entities_.contains(id) || !registry_.valid(entity) || registry_.all_of<entity_id_component_t>(entity))
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
