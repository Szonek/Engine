#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
// Local to the parent (the world when the game object has no parent). The world transform is derived from it by the
// scene, see engineGameObjectGetWorldTransform.
typedef struct _engine_transform_component_t
{
    float position[3];
    float scale[3];
    float rotation[4]; // quaternion
} engine_transform_component_t;

#ifdef __cplusplus
}
#endif // cpp