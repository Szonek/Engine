#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef struct _engine_rect_tranform_component_t
{
    float position[2];
    float scale[2];
    float local_to_world[16];
} engine_rect_tranform_component_t;

#ifdef __cplusplus
}
#endif // cpp