#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdint.h>

typedef struct _engine_rigid_body_component_t
{
    uint32_t mass;  // in kg
    bool use_gravity;
} engine_rigid_body_component_t;

#ifdef __cplusplus
}
#endif // cpp