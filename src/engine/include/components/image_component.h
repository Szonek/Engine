#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef uint32_t engine_texture2d_t;

typedef struct _engine_image_component_t
{
    engine_texture2d_t source_image;
    float color[4];
} engine_image_component_t;

#ifdef __cplusplus
}
#endif // cpp