#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef struct _engine_rect_tranform_component_t
{
    float position_min[2]; // x_min, y_min (normalized)
    float position_max[2]; // x_max, y_max (normalized)
    float local_to_world[16];
} engine_rect_tranform_component_t;

#ifdef __cplusplus
}
#endif // cpp