#pragma once
#include "prefab_instances.h"
#include "type_registry.h"
#include "../entity_id_registry.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
    // the type of the value the data asset holds, nullptr when the handle doesn't name one (any more)
    virtual const TypeInfo* get_data_asset_type(std::uint32_t handle) const = 0;
};

// Scene files hold authored data only: every game object with a persistent ID and its components which have a type
// in the registry (internal components, derived data, are never registered), as JSON:
//   { "version": 1, "types": { "transform": 1, ... }, "entities": [ { "id": 3, "components": { "transform": { ... } } } ] }
// A prefab instance is saved as its root with the prefab's asset ID and what differs from the prefab since it was
// created, by node path ("" for the root), each kind under its own key (like Unity's prefab instances): "components"
// whole (the ones added to the instance, and the root's name and transform, always), "overrides" of the ones the
// prefab gave it which changed, only the fields which differ from the prefab's value (like Unity's prefab overrides; an
// array is one field, a union is one value: its tag and its whole case), "removed" the names of the ones the prefab gave
// it which were removed. The rest is not written, the prefab creates it again when the file is loaded, so a change to
// the prefab shows up where nothing overrides it:
//   { "id": 4, "prefab": "characters/Rogue.glb", "nodes": { "": { "components": { "name": { ... }, "transform": { ... } } },
//     "Body": { "overrides": { "mesh": { "disable": true } } }, "Body/Hand": { "components": { "collider": { ... } }, "removed": [ "material" ] } } }
// The other game objects of an instance have the hashed IDs of their node paths (EntityIdRegistry::hashed_id), which
// references to them use. A prefab whose asset ID ends in ".json" is another scene file (ScenePrefab): its node paths are
// the IDs of the game objects in that file ("12", "12/Body" for a node of a model's instance there).
// Game objects are sorted by persistent ID, components are in registration order and fields in description order,
// so saving the same scene gives the same bytes. Every field is written (but in overrides), defaults too: a file
// doesn't change meaning when a default changes. Enums are written by name, asset references as asset IDs, game object references as
// persistent IDs (null for none); a union writes only its active case, under the name of its tag's value.
inline constexpr std::uint32_t SCENE_FILE_VERSION = 1;

// The scene as a scene file. Transient game objects aren't saved, so a reference to one is saved as null (logged).
// Game objects removed from what a prefab created on its instance can't be saved yet: they are logged, once per
// instance.
std::string save_scene(const TypeRegistry& types, const entt::registry& registry, const EntityIdRegistry& ids, const IAssetResolver& assets,
    const IPrefabInstances& prefabs);
// Only these game objects as a scene file, for a snapshot loaded back into the same scene (e.g. undo of a delete: a game
// object and everything parented under it). Transient ones are left out, as in save_scene; references to game objects
// which aren't saved are written like the others, by persistent ID. A game object removed from what a prefab created
// can't be kept, it is logged, as when a scene is saved.
std::string save_game_objects(const TypeRegistry& types, const entt::registry& registry, const EntityIdRegistry& ids, const IAssetResolver& assets,
    const IPrefabInstances& prefabs, std::span<const entt::entity> game_objects);

// Whether `json` can be loaded as a scene file (JSON, a version this engine reads, an entity list), logged when not.
bool check_scene_file(std::string_view json, std::string_view file_name);

// Where the file of a scene prefab is loaded to: its game objects become the game objects of an instance (see
// ScenePrefab).
struct SceneInstanceTarget
{
    // the instance's root: the file's game objects without a parent are parented to it
    entt::entity root = entt::null;
    // the IDs of the instance's game objects: the hashed IDs of their node paths (none when the instance is transient).
    // The node path of a game object of the file is its persistent ID in the file ("12"), of a game object of a prefab
    // instance in the file the ID of the instance's root in the file and its node path there ("12/Body").
    InstanceIds ids;
    // game objects created already, without a persistent ID, which become the ones with these hashed IDs (to restore
    // their handles); the others are created
    const EntityHandles* node_handles = nullptr;
    // filled with the instance's game objects (but its root) and their node paths
    std::vector<PrefabInstanceNode> nodes;
};

struct SceneLoadOptions
{
    // the handles to create the file's game objects with, by persistent ID (hashed IDs for the game objects of a prefab
    // instance), so references to them which outlived them (in other game objects, in the editor's undo history) find
    // them again. They are taken before any other game object is created; a game object whose handle is in use already
    // gets a new one.
    EntityHandles handles;
    // a reference to a game object which isn't in the file finds the registry's game object with that persistent ID
    // (e.g. the parent of a restored subtree); otherwise it is none
    bool find_references_outside_file = false;
    // every game object gets a new persistent ID instead of the file's, for a copy of game objects the registry has
    // already (the editor's duplicate); the file's references to them find the copies, the game objects of a prefab
    // instance get the hashed IDs of the new root ID
    bool new_ids = false;
    // when given, filled with the loaded game objects by the persistent IDs the file gives them (hashed IDs of the
    // file's root ID for the game objects of a prefab instance), e.g. to find a copy
    EntityHandles* loaded = nullptr;
    // when given, the file is a scene prefab's, loaded into an instance: its game objects get the IDs of the instance's
    // game objects instead of the file's (handles, new_ids and loaded aren't used, references are never looked up
    // outside the file)
    SceneInstanceTarget* instance = nullptr;
};

// Adds the game objects of a scene file to the registry. The file is read and checked first: when it can't be used
// (not JSON, no version or a newer one than this engine's, no entity list) the load fails and the registry is not
// changed. Otherwise every problem inside it is logged (with the file name, persistent ID, component and field) and
// the load goes on: an entity without a valid ID is skipped, an unknown component or field is ignored, a missing field
// keeps the component's default, a string too long is cut, a reference to a game object which isn't in the file is
// none (unless SceneLoadOptions say otherwise), an asset which can't be found is none.
// Game objects keep the persistent IDs of the file (unless SceneLoadOptions::new_ids); one whose ID is used already
// gets a new one (references in the file still point at it). Prefab instances are instantiated once every game object of the file exists; a prefab which
// can't be found leaves its root without the prefab's game objects (what is saved whole of the root is loaded), and
// what the file has for a node its model doesn't have (any more) is ignored, as are references to it. Components are
// added the way the C API does (add with their defaults, then update with the file's values), so the scene's listeners
// run, one component type after the other in registration order. On the game objects of an instance, a component
// saved whole which the prefab gives (by now) is read over the prefab's value; an override is read over the prefab's
// value, so the fields the file doesn't have keep the prefab's, and is ignored (logged) when the prefab doesn't give
// that component (any more); a removed component is removed when the prefab gives it.
bool load_scene(std::string_view json, std::string_view file_name, const TypeRegistry& types, entt::registry& registry, EntityIdRegistry& ids, IAssetResolver& assets,
    IPrefabInstances& prefabs, const SceneLoadOptions& options = {});

// Data asset files hold one value of a game-defined struct type (see DataAsset) and the version of the type it was
// written for (the first one when a file written by hand leaves it out), its fields written like a component's in a
// scene file:
//   { "version": 1, "type": "enemy_type", "type_version": 1, "value": { "max_hp": 20, ... } }
inline constexpr std::uint32_t DATA_ASSET_FILE_VERSION = 1;

// The value of `type` (one without game object references, holds_game_object) as a data asset file.
std::string save_data_asset(const TypeInfo& type, const std::byte* value, const IAssetResolver& assets);

struct DataAssetValue
{
    const TypeInfo* type = nullptr;
    std::unique_ptr<ValueBuffer> value;
};

// Reads a data asset file: the type it names, which is a game-defined struct type without game object references, and
// its value, which starts as the type's default value (TypeInfo::default_value), so a field the file doesn't have keeps
// it. Nothing (logged) when the file can't be used: not JSON, a version this engine doesn't read, an unknown type or one
// a data asset can't hold, a type version newer than the type's, no value object. An older type version is logged and
// read like a component of an older version in a scene file. Problems inside the value are logged and skipped, as in a scene file.
std::optional<DataAssetValue> load_data_asset(std::string_view json, std::string_view file_name, const TypeRegistry& types, IAssetResolver& assets);
}  // namespace engine
