#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
typedef uint32_t engine_animation_clip_t;
#define ENGINE_ANIMATIONS_CLIPS_MAX_COUNT 16
typedef enum _engine_animation_clip_state_t
{
    ENGINE_ANIMATION_CLIP_STATE_NOT_PLAYING,
    ENGINE_ANIMATION_CLIP_STATE_PLAYING,
} engine_animation_clip_state_t;

typedef struct _engine_animation_component_t
{
    engine_animation_clip_t animations_array[ENGINE_ANIMATIONS_CLIPS_MAX_COUNT];
    engine_animation_clip_state_t animations_state[ENGINE_ANIMATIONS_CLIPS_MAX_COUNT];
    float animations_dt[ENGINE_ANIMATIONS_CLIPS_MAX_COUNT];
} engine_animation_component_t;

#ifdef __cplusplus
}
#endif // cpp