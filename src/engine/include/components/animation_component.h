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

typedef enum _engine_animation_channel_interpolation_type_t
{
    ENGINE_ANIMATION_CHANNEL_INTERPOLATION_TYPE_LINEAR,
    //ENGINE_ANIMATION_CHANNEL_INTERPOLATION_TYPE_STEP,
    //ENGINE_ANIMATION_CHANNEL_INTERPOLATION_TYPE_CUBIC_SPLINE,
} engine_animation_channel_interpolation_type_t;

#define ENGINE_ANIMATION_CHANNEL_MAX_DATA_SIZE 256
typedef struct _engine_animation_channel_t
{
    engine_animation_channel_interpolation_type_t interpolation_type;
    float timestamps[ENGINE_ANIMATION_CHANNEL_MAX_DATA_SIZE];
    uint32_t timestamps_count;

    float data[ENGINE_ANIMATION_CHANNEL_MAX_DATA_SIZE];  // for vec3: x,y,z; for rotation quaternion: x,y,z,w
    size_t data_count;
} engine_animation_channel_t;

typedef struct _engine_animation_clip_desc_t
{
    engine_animation_channel_t channel_translation;
    engine_animation_channel_t channel_rotation;
    engine_animation_channel_t channel_scale;
    float animation_dt;
} engine_animation_clip_desc_t;

typedef struct _engine_animation_clip_component_t
{
    engine_animation_clip_desc_t clips_array[ENGINE_ANIMATIONS_CLIPS_MAX_COUNT];
} engine_animation_clip_component_t;

#ifdef __cplusplus
}
#endif // cpp