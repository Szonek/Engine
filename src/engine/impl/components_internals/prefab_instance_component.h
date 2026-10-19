#pragma once
#include <cstdint>

namespace engine
{
// Marks the root game object of a prefab instance and links it to the prefab asset it was instantiated from.
struct prefab_instance_internal_component_t
{
    std::uint32_t prefab;
};
}  // namespace engine
