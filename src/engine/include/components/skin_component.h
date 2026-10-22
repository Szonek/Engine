#pragma once

#include "engine_string.h"

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdint.h>

typedef uint32_t engine_skin_t;
typedef uint32_t engine_game_object_t;
// The game object owns one instance of the skin asset: its pose, written by the animation controller on the same
// game object. Skinned meshes and joint attachments reference this game object.
typedef struct _engine_skin_component_t
{
    engine_skin_t skin;
} engine_skin_component_t;

// use joint attchment component to attach a game object to joint to follow it's position
typedef struct _engine_joint_attachment_component_t
{
    // the game object with the skin component whose pose the joint is read from
    engine_game_object_t skin;
    // name of the joint to attach to: the engine's string, valid while the component exists (do not destroy it). Change it
    // with engineStringSet; an update carrying another string copies its value and keeps pointing at the engine's.
    engine_string_t* joint_name;
} engine_joint_attachment_component_t;

#ifdef __cplusplus
}
#endif // cpp
