#pragma once
#include "serialization/prefab_instances.h"

#include <entt/entt.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace engine
{
class Scene;

// A prefab asset: something the scene can stamp out as game objects any number of times, named by its asset ID. A
// model (ModelPrefab, "characters/Rogue.glb", a path under assets/models) or a scene file (ScenePrefab,
// "rooms/room_a.json", a path under assets/scenes): is_scene_prefab_id tells them apart.
class Prefab
{
public:
    enum class Instance
    {
        ePersistent,  // every game object gets a persistent ID
        eTransient    // no game object gets one (Scene::create_transient_entity)
    };

    Prefab() = default;
    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;
    Prefab(Prefab&&) = delete;
    Prefab& operator=(Prefab&&) = delete;
    virtual ~Prefab() = default;

    // its asset ID
    virtual const std::string& get_name() const = 0;

    // Creates the game objects of the prefab in `scene` and returns them with their node paths, the root first (its
    // path is ""), or nothing (logged) when it can't. Every other game object is under the root.
    // `root`, when given, is the game object which becomes the root (a scene file's, with the ID of the file). In a
    // persistent instance the other game objects get the hashed IDs of their node paths (InstanceIds; nullptr: the
    // root's ID and no prefix). `node_handles`: see Scene::create_prefab_node_entity.
    virtual std::vector<PrefabInstanceNode> instantiate(Scene& scene, Instance instance, entt::entity root = entt::null, const EntityHandles* node_handles = nullptr,
        const InstanceIds* ids = nullptr) const = 0;
};

// Whether a prefab's asset ID names a scene file (it ends in ".json", in any case), not a model.
inline bool is_scene_prefab_id(std::string_view asset_id)
{
    constexpr std::string_view extension = ".json";
    return asset_id.size() >= extension.size()
        && std::ranges::equal(asset_id.substr(asset_id.size() - extension.size()), extension,
            [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; });
}
}  // namespace engine
