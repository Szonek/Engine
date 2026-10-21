#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

typedef enum _engine_nav_modifier_type_t
{
    // included like a collider without a modifier: its slope decides what on it is walkable. Brings back the children of
    // a not walkable or ignored game object.
    ENGINE_NAV_MODIFIER_TYPE_WALKABLE = 0,
    // the collider is an obstacle: nothing on it is walkable
    ENGINE_NAV_MODIFIER_TYPE_NOT_WALKABLE,
    // the collider is left out of the nav mesh build
    ENGINE_NAV_MODIFIER_TYPE_IGNORE,
} engine_nav_modifier_type_t;

// How the colliders of a game object and its children (unless they have a modifier of their own) contribute to the nav
// mesh. Without a modifier, a static collider is included and its slope decides what on it is walkable. Colliders of
// kinematic rigid bodies, dynamic rigid bodies with mass and triggers are never included. A new modifier is ENGINE_NAV_MODIFIER_TYPE_WALKABLE, so it
// changes nothing until another type is picked.
typedef struct _engine_nav_modifier_component_t
{
    engine_nav_modifier_type_t type;
} engine_nav_modifier_component_t;

#ifdef __cplusplus
}
#endif // cpp
