#pragma once
#include "entity_id_registry.h"

#include <entt/entt.hpp>

#include <cstdint>

namespace engine
{
// A game object the editor keeps referring to (its undo commands, the selection), like Unity's GlobalObjectId or
// Unreal's soft object paths: by its persistent ID, looked up each time it is used, so it finds the game object again
// under its new handle after Stop loaded the scene saved at Play back, or after undo brought a deleted one back. A
// transient game object (no persistent ID, a runtime one) is referred to by its handle, which finds nothing once it
// is destroyed.
class GameObjectRef
{
public:
    // none
    GameObjectRef() = default;

    // entt::null gives none
    static GameObjectRef of(const EntityIdRegistry& ids, entt::entity entity)
    {
        GameObjectRef ref;
        if (entity != entt::null)
        {
            ref.id_ = ids.get_id(entity);
            ref.handle_ = ref.id_ == EntityIdRegistry::INVALID_ID ? entity : entt::entity{ entt::null };
        }
        return ref;
    }

    // the game object now, entt::null when there is none (any more)
    entt::entity resolve(const entt::registry& registry, const EntityIdRegistry& ids) const
    {
        if (id_ != EntityIdRegistry::INVALID_ID)
        {
            return ids.find(id_);
        }
        return handle_ != entt::null && registry.valid(handle_) ? handle_ : entt::entity{ entt::null };
    }

    bool is_none() const { return id_ == EntityIdRegistry::INVALID_ID && handle_ == entt::null; }
    bool operator==(const GameObjectRef&) const = default;

private:
    std::uint64_t id_ = EntityIdRegistry::INVALID_ID;
    entt::entity handle_ = entt::null;
};
}  // namespace engine
