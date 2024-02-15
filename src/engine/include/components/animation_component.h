#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef uint32_t engine_animation_clip_t;
#define ENGINE_ANIMATION_CLIP_MAX 128

typedef struct _engine_animation_component_t
{
    engine_animation_clip_t animations_array[ENGINE_ANIMATION_CLIP_MAX];
} engine_animation_component_t;

#ifdef __cplusplus
}
#endif // cpp