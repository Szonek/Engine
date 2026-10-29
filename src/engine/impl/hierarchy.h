#pragma once
#include <entt/entt.hpp>

namespace engine
{
// Keeps the children lists (children_internal_component_t) in step with the parent components of `registry`: a child
// is listed under its parent when its parent component is changed to point at it (a new component has no parent yet,
// see initialize_parent_component), moves to the new parent's list when it is changed to point at another one (the
// parent storage reports the previous parent, see parent_storage.h; the child keeps its place when the parent stays
// the same), and leaves the list when its parent component is removed or it is destroyed. A game object whose children
// list is destroyed (it is destroyed) leaves its children without a parent component.
void connect_hierarchy(entt::registry& registry);
}  // namespace engine
