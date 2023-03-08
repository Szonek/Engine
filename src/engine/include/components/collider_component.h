#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdint.h>

typedef enum _engine_collider_type_t
{
    ENGINE_COLLIDER_TYPE_BOX = 0,
    ENGINE_COLLIDER_TYPE_SPHERE = 1,
} engine_collider_type_t;

typedef struct _engine_box_collider_t
{
    float center[3];
    float size[3];
} engine_box_collider_t;

typedef struct _engine_sphere_collider_t
{
    float center[3];
    float radius;
} engine_sphere_collider_t;

typedef struct _engine_collider_component_t
{
    engine_collider_type_t type;
    union 
    {
        engine_box_collider_t box;
        engine_sphere_collider_t sphere;
    } collider;
} engine_collider_component_t;

#ifdef __cplusplus
}
#endif // cpp