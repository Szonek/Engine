#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

#include <stdint.h>

typedef enum _engine_camera_projection_type_t
{
    ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC = 0,
    ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE = 1
} engine_camera_projection_type_t;

// values in range <0; 1>
typedef struct _engine_viewport_rect_component_t
{
    float x;  
    float y;
    float width;
    float height;
} engine_viewport_rect_component_t;

typedef struct _engine_camera_direction_t
{
    float up[3];
    float right[3];
    float front[3];
} engine_camera_direction_t;

typedef struct _engine_camera_component_t
{
    bool enabled;
    engine_camera_projection_type_t type;
    union
    {
        float orthographics_scale;
        float perspective_fov;
    } type_union;
    float target[3];
    engine_viewport_rect_component_t viewport_rect;
    engine_camera_direction_t direction;
    float pitch;
    float yaw;
    float roll;
    float clip_plane_near;
    float clip_plane_far;
} engine_camera_component_t;



#ifdef __cplusplus
}
#endif // cpp