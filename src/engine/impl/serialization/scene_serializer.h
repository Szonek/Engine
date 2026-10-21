#pragma once
#include "type_registry.h"
#include "../entity_id_registry.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <string>
#include <string_view>

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
// Game objects are sorted by persistent ID, components are in registration order and fields in description order,
// so saving the same scene gives the same bytes. Every field is written, defaults too: a file doesn't change meaning
// when a default changes. Enums are written by name, asset references as asset IDs, game object references as
// persistent IDs (null for none); a union writes only its active case, under the name of its tag's value.
inline constexpr std::uint32_t SCENE_FILE_VERSION = 1;

// The scene as a scene file. Transient game objects aren't saved, so a reference to one is saved as null (logged).
std::string save_scene(const TypeRegistry& types, const entt::registry& registry, const EntityIdRegistry& ids, const IAssetResolver& assets);

// Adds the game objects of a scene file to the registry. The file is read and checked first: when it can't be used
// (not JSON, no version or a newer one than this engine's, no entity list) the load fails and the registry is not
// changed. Otherwise every problem inside it is logged (with the file name, persistent ID, component and field) and
// the load goes on: an entity without a valid ID is skipped, an unknown component or field is ignored, a missing field
// keeps the component's default, a string too long is cut, a reference to a game object which isn't in the file is
// none, an asset which can't be found is none.
// Game objects keep the persistent IDs of the file; one whose ID is used already gets a new one (references in the
// file still point at it). Components are added the way the C API does (add with their defaults, then update with the
// file's values), so the scene's listeners run, one component type after the other in registration order.
bool load_scene(std::string_view json, std::string_view file_name, const TypeRegistry& types, entt::registry& registry, EntityIdRegistry& ids, IAssetResolver& assets);
}  // namespace engine
