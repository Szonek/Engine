#pragma once
#include "type_registry.h"

#include <engine.h>

namespace engine
{
// Registers a type the game describes through the C API (engineComponentTypeRegister, engineStructTypeRegister), or
// nullptr when the description has a mistake (each one is logged). A component is stored by its name, in a storage of
// the registry holding untyped values of its size (DataBlob: one storage per type, a few sizes up to
// ENGINE_COMPONENT_TYPE_MAX_SIZE), so it works like the engine's components: added with its default value, read and
// updated as a copy, destroyed with its game object. Nothing in the engine reacts to them (no listeners: the registry's
// observers wouldn't see these storages anyway, only the storages' own signals would).
const TypeInfo* register_game_type(TypeRegistry& registry, const engine_type_desc_t& desc, bool component);
}  // namespace engine
