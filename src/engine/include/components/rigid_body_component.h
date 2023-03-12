#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdint.h>

typedef struct _engine_rigid_body_component_t
{
    uint32_t mass;  // in kg
    
    float linear_velocity[3];

} engine_rigid_body_component_t;

#ifdef __cplusplus
}
#endif // cpp