#pragma once
#include "type_registry.h"

namespace engine
{
// Registers the engine's components which scene files hold, and the structs inside them. The order is the order
// components are loaded in (see TypeRegistry). Components without a description are not saved: internal ones never
// are (derived data), and the public ones which aren't described yet are listed in builtin_types.cpp.
void register_builtin_types(TypeRegistry& registry);
}  // namespace engine
