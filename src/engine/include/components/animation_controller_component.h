#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

#include <stdint.h>

typedef uint32_t engine_animation_set_t;
// Plays the animation set on the skin of the same game object. The engine creates the controller when the component
// gets an animation set (and the game object has a skin) and destroys it with the component; get it with
// engineGameObjectGetAnimationController.
typedef struct _engine_animation_controller_component_t
{
    engine_animation_set_t animations;
} engine_animation_controller_component_t;

#ifdef __cplusplus
}
#endif // cpp
