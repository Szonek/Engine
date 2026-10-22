#pragma once
#include "type_registry.h"

#include <engine.h>

#include <cstddef>

namespace engine
{
// Registers a type the game describes through the C API (engineComponentTypeRegister, engineStructTypeRegister), or
// nullptr when the description has a mistake (each one is logged). A component is stored by its name, in a storage of
// the registry holding untyped values in slots of data_slot_size bytes (DataBlob: one storage per type, a packed array
// like the engine's components), so it works like the engine's components: added with its default value, read and
// updated as a copy, destroyed with its game object. Nothing in the engine reacts to them: no listener is connected to
// these storages' signals (and a reactive storage would have to watch each type's storage by its id).
const TypeInfo* register_game_type(TypeRegistry& registry, const engine_type_desc_t& desc, bool component);

// The alignment of the slots game components are stored in: the most a game type may ask for.
inline constexpr std::size_t DATA_SLOT_ALIGNMENT = ENGINE_COMPONENT_TYPE_MAX_ALIGNMENT;
static_assert(ENGINE_COMPONENT_TYPE_MAX_SIZE % DATA_SLOT_ALIGNMENT == 0);

// The slot a game component of `size` bytes is stored in: the next multiple of DATA_SLOT_ALIGNMENT, so a component
// wastes less than one alignment. An EnTT storage holds one compile-time type, so there is one slot type per size up to
// ENGINE_COMPONENT_TYPE_MAX_SIZE.
constexpr std::size_t data_slot_size(std::size_t size)
{
    return (size + DATA_SLOT_ALIGNMENT - 1) / DATA_SLOT_ALIGNMENT * DATA_SLOT_ALIGNMENT;
}
}  // namespace engine
