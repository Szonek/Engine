#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdbool.h>

// A character moved with engineCharacterControllerMove (like Unity's CharacterController): a capsule standing on its game
// object's origin which slides along walls, walks up steps and walkable slopes, sticks to the ground and falls with the
// physics' gravity. Sizes are in the game object's space: they are scaled by its world scale, like collider sizes.
// The capsule is a kinematic body: dynamic bodies are pushed by it, triggers and kinematic bodies (hitboxes) see it.
// It replaces a collider and rigid body: don't add them to the same game object. The game object must have no parent.
typedef struct _engine_character_controller_component_t
{
    float radius;
    float height;       // of the whole capsule, at least 2 * radius
    float step_height;  // ledges up to this height are walked onto
    float max_slope;    // in degrees: steeper ground is not walkable, it blocks like a wall
} engine_character_controller_component_t;

typedef struct _engine_character_controller_move_result_t
{
    bool grounded;     // stands on walkable ground after the move (not on an edge or a too steep slope)
    bool hit_wall;     // the move was blocked sideways (it slid along what blocked it)
    bool hit_ceiling;  // an upward move was blocked
} engine_character_controller_move_result_t;

#ifdef __cplusplus
}
#endif // cpp
