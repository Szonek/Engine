#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef struct _engine_transform_component_t
{
    float position[3];
    float scale[3];
    float rotation[4]; // quaternion
    float local_to_world[16];
} engine_transform_component_t;

#ifdef __cplusplus
}
#endif // cpp