#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

#include <stdint.h>

#include <stdbool.h>

#define ENGINE_ANIMATION_CONTROLLER_MAX_LAYERS 4
#define ENGINE_ANIMATION_LAYER_MAX_JOINT_WEIGHTS 8
#define ENGINE_ANIMATION_JOINT_NAME_MAX_LENGTH 64

typedef uint32_t engine_animation_set_t;

typedef enum _engine_animation_layer_mode_t
{
    ENGINE_ANIMATION_LAYER_MODE_OVERRIDE,
    ENGINE_ANIMATION_LAYER_MODE_ADDITIVE
} engine_animation_layer_mode_t;

// the layer's weight for a joint and all its descendants; applied in order, so later entries refine earlier ones
typedef struct _engine_animation_layer_joint_weight_t
{
    char joint_name[ENGINE_ANIMATION_JOINT_NAME_MAX_LENGTH];
    float weight;
} engine_animation_layer_joint_weight_t;

// How a layer blends. weight and looping are starting values: engineAnimationControllerLayerSetWeight /
// LayerSetLooping change them while playing.
typedef struct _engine_animation_layer_desc_t
{
    engine_animation_layer_mode_t mode;
    float weight;
    bool looping;
    // none: every joint has weight 1
    uint32_t joint_weights_count;
    engine_animation_layer_joint_weight_t joint_weights[ENGINE_ANIMATION_LAYER_MAX_JOINT_WEIGHTS];
} engine_animation_layer_desc_t;

// Plays the animation set on the skin of the same game object, with these layers (layer id = index; a new component
// has one override layer with weight 1). The engine creates the controller when the component gets an animation set
// (and the game object has a skin) and destroys it with the component; updating only the layers reconfigures them on
// the same controller, which keeps its animation events. The engineAnimationController* functions act on the controller
// through the game object.
typedef struct _engine_animation_controller_component_t
{
    engine_animation_set_t animations;
    uint32_t layers_count;
    engine_animation_layer_desc_t layers[ENGINE_ANIMATION_CONTROLLER_MAX_LAYERS];
} engine_animation_controller_component_t;

#ifdef __cplusplus
}
#endif // cpp
