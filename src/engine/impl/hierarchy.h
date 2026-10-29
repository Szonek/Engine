#pragma once
#include <entt/entt.hpp>

namespace engine
{
// The parent whose children list (children_internal_component_t) holds this game object, like the back pointer to its
// parent that Unity's Transform and Godot's Node keep. A changed parent component can't tell which parent it had, so
// the child is taken out of this one's list. Derived, never saved; entt::null while the game object is listed nowhere.
struct listed_parent_internal_component_t
{
    entt::entity parent = entt::null;
};

// Keeps the children lists (children_internal_component_t) in step with the parent components of `registry`: a child
// is listed under its parent when its parent component is updated to point at it (a new component has no parent yet,
// see initialize_parent_component), moves to the new parent's list when it is updated to point at another one (keeping
// its place when the parent stays the same), and leaves the list when its parent component is removed or it is
// destroyed. A game object whose children list is destroyed (it is destroyed) leaves its children without a parent
// component.
void connect_hierarchy(entt::registry& registry);
}  // namespace engine
