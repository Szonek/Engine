#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef uint32_t engine_animation_clip_t;
#define ENGINE_ANIMATION_CLIP_MAX 128
typedef enum _engine_animation_clip_state_t
{
    ENGINE_ANIMATION_CLIP_STATE_NOT_PLAYING,
    ENGINE_ANIMATION_CLIP_STATE_PLAYING,
    ENGINE_ANIMATION_CLIP_STATE_START_PLAYING,
} engine_animation_clip_state_t;

typedef struct _engine_animation_component_t
{
    engine_animation_clip_t animations_array[ENGINE_ANIMATION_CLIP_MAX];
    engine_animation_clip_state_t animations_state[ENGINE_ANIMATION_CLIP_MAX];
} engine_animation_component_t;

#ifdef __cplusplus
}
#endif // cpp