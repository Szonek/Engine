#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

typedef struct _engine_animation_controller_t engine_animation_controller_t;
typedef struct _engine_animation_controller_component_t
{
    engine_animation_controller_t* animation_controller;
} engine_animation_controller_component_t;

#ifdef __cplusplus
}
#endif // cpp
