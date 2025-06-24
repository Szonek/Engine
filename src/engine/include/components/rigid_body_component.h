#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdint.h>

typedef struct _engine_rigid_body_component_t
{
    float mass;  // in kg
} engine_rigid_body_component_t;

#ifdef __cplusplus
}
#endif // cpp