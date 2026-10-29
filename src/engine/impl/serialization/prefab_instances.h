#pragma once
#include "type_registry.h"
#include "../entity_id_registry.h"

#include <entt/entt.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace engine
{
// A component the prefab gave a node of an instance, with the value it had then.
struct PrefabComponentValue
{
    const TypeInfo* type;
    ValueBuffer value;
};

// A game object of a prefab instance: the model's node at `path` (the names of the nodes from the root down, joined by
// '/'; "" is the root, see Prefab).
struct PrefabInstanceNode
{
    std::string path;
    entt::entity entity;
    // the registered components the prefab gave it, as they were right after the instance was created: what a scene
    // file doesn't need to save, and what its overrides are the differences from
    std::vector<PrefabComponentValue> created;
};

// Where the persistent IDs of the game objects of a prefab instance (but its root) come from: each gets
// EntityIdRegistry::hashed_id(root_id, prefix + its node path). An instance has its root's ID and no prefix; an instance
// inside the instance of a scene prefab has that instance's (the outermost one's) with the node path of its own root
// there and '/' as the prefix, so every game object of a scene prefab's instance, however deep, has the hashed ID of
// its node path in it. root_id INVALID_ID: the instance is transient, none of its game objects gets an ID.
struct InstanceIds
{
    std::uint64_t root_id = EntityIdRegistry::INVALID_ID;
    std::string prefix;
};

// What scene files need to know about a prefab instance: the prefab's asset ID (the model's path) and its game objects,
// the root first.
struct PrefabInstanceInfo
{
    std::string prefab;
    std::vector<PrefabInstanceNode> nodes;
};

// Fills the `created` list of every node with the registered components the node has now (call it right after the
// instance was created).
void capture_prefab_components(const TypeRegistry& types, const entt::registry& registry, PrefabInstanceInfo& instance);

// The prefab instances of a scene, for scene files.
class IPrefabInstances
{
public:
    virtual ~IPrefabInstances() = default;
    // the instance whose root is `root`, or nullptr when it is none
    virtual const PrefabInstanceInfo* find_instance(entt::entity root) const = 0;
    // Instantiates the prefab with this asset ID (loading it when needed) with `root` as its root game object; the
    // other game objects get the hashed IDs `ids` gives them (none when it is transient). nullptr (logged) when the
    // prefab can't be found or instantiated. `node_handles`: game objects created already, without a persistent ID,
    // which become the nodes with these hashed IDs (to restore their handles); the others are created.
    virtual const PrefabInstanceInfo* instantiate(std::string_view prefab_asset_id, entt::entity root, const EntityHandles& node_handles, const InstanceIds& ids) = 0;
};
}  // namespace engine
