#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdint.h>

typedef enum _engine_rigid_body_type_t
{
    // moved by the physics: gravity, collisions, forces. Move it with forces, not by setting its transform every frame.
    // Without mass it is static.
    ENGINE_RIGID_BODY_TYPE_DYNAMIC = 0,
    // moved by its transform (a script, an animation, a parent), never by the physics: it pushes dynamic bodies and is
    // seen by triggers and other kinematic bodies
    ENGINE_RIGID_BODY_TYPE_KINEMATIC,
    // never moves (moving it teleports it): level geometry. Also for colliders of children which would be kinematic
    // because of their parent.
    ENGINE_RIGID_BODY_TYPE_STATIC,
} engine_rigid_body_type_t;

// A collider without a rigid body is static, or kinematic when it has a parent or joint attachment (it moves with
// them). A rigid body states the type explicitly.
typedef struct _engine_rigid_body_component_t
{
    engine_rigid_body_type_t type;
    float mass;  // in kg, only used by dynamic bodies
} engine_rigid_body_component_t;

#ifdef __cplusplus
}
#endif // cpp