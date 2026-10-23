#pragma once
#include "prefab_instances.h"
#include "type_registry.h"
#include "../entity_id_registry.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace engine
{
// Translates asset handles to asset IDs (the atlas names, see asset_id.h) and back, for scene files.
class IAssetResolver
{
public:
    virtual ~IAssetResolver() = default;
    // "" when the handle doesn't name an asset (any more)
    virtual std::string get_asset_id(AssetKind kind, std::uint32_t handle) const = 0;
    // The asset with this ID, loaded when needed. ENGINE_INVALID_OBJECT_HANDLE (logged) when it can't be found.
    virtual std::uint32_t resolve(AssetKind kind, std::string_view asset_id) = 0;
};

// Scene files hold authored data only: every game object with a persistent ID and its components which have a type
// in the registry (internal components, derived data, are never registered), as JSON:
//   { "version": 1, "types": { "transform": 1, ... }, "entities": [ { "id": 3, "components": { "transform": { ... } } } ] }
// A prefab instance is saved as its root with the prefab's asset ID, the root's name and transform and the components
// added to the instance after it was created, on the root or on its other game objects (under their node paths); the
// rest of what its prefab created is not written, the prefab creates it again when the file is loaded:
//   { "id": 4, "prefab": "characters/Rogue.glb", "components": { "name": { ... }, "transform": { ... } }, "nodes": { "Body/Hand": { "collider": { ... } } } }
// The other game objects of an instance have the hashed IDs of their node paths (EntityIdRegistry::hashed_id), which
// references to them use.
// Game objects are sorted by persistent ID, components are in registration order and fields in description order,
// so saving the same scene gives the same bytes. Every field is written, defaults too: a file doesn't change meaning
// when a default changes. Enums are written by name, asset references as asset IDs, game object references as
// persistent IDs (null for none); a union writes only its active case, under the name of its tag's value.
inline constexpr std::uint32_t SCENE_FILE_VERSION = 1;

// The scene as a scene file. Transient game objects aren't saved, so a reference to one is saved as null (logged).
// Changes to the components a prefab created on its instance can't be saved yet: they are logged, once per instance.
std::string save_scene(const TypeRegistry& types, const entt::registry& registry, const EntityIdRegistry& ids, const IAssetResolver& assets,
    const IPrefabInstances& prefabs);
// Only these game objects and everything parented under them, as a scene file (e.g. the snapshot that brings back a
// destroyed game object). References to game objects outside of them are saved like the others, by persistent ID.
std::string save_game_objects(const TypeRegistry& types, const entt::registry& registry, const EntityIdRegistry& ids, const IAssetResolver& assets,
    const IPrefabInstances& prefabs, std::span<const entt::entity> roots);

struct SceneLoadOptions
{
    // the handles to create the file's game objects with, by persistent ID, so references to them which outlived them
    // (in other game objects, in the editor's undo history) find them again; a handle in use already gets a new one.
    // The other game objects of a prefab instance are created by its prefab and get new handles.
    std::unordered_map<std::uint64_t, entt::entity> handles;
    // a reference to a game object which isn't in the file finds the registry's game object with that persistent ID
    // (e.g. the parent of a restored subtree); otherwise it is none
    bool find_references_outside_file = false;
};

// Adds the game objects of a scene file to the registry. The file is read and checked first: when it can't be used
// (not JSON, no version or a newer one than this engine's, no entity list) the load fails and the registry is not
// changed. Otherwise every problem inside it is logged (with the file name, persistent ID, component and field) and
// the load goes on: an entity without a valid ID is skipped, an unknown component or field is ignored, a missing field
// keeps the component's default, a string too long is cut, a reference to a game object which isn't in the file is
// none (unless SceneLoadOptions say otherwise), an asset which can't be found is none.
// Game objects keep the persistent IDs of the file; one whose ID is used already gets a new one (references in the
// file still point at it). Prefab instances are instantiated once every game object of the file exists; a prefab which
// can't be found leaves its root without the prefab's game objects, and the components of a node its model doesn't
// have (any more) are ignored, as are references to it. Components are added the way the C API does (add with their
// defaults, then update with the file's values), so the scene's listeners run, one component type after the other in
// registration order.
bool load_scene(std::string_view json, std::string_view file_name, const TypeRegistry& types, entt::registry& registry, EntityIdRegistry& ids, IAssetResolver& assets,
    IPrefabInstances& prefabs, const SceneLoadOptions& options = {});
}  // namespace engine
