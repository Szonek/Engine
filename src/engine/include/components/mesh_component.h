#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

#include <stdint.h>

typedef uint32_t engine_geometry_t;
typedef struct _engine_skin_t engine_skin_t;

typedef struct _engine_mesh_component_t
{
	engine_geometry_t geometry;
    bool disable;
    // Screen-door (dithered) fade: 0 - opaque (default), 1 - invisible. Faded mesh still casts shadows.
    // Used i.e. to fade walls between the camera and the player.
    float fade;
} engine_mesh_component_t;

typedef struct _engine_skinned_mesh_component_t
{
    engine_geometry_t geometry;
    engine_skin_t* skin;
    bool disable;
} engine_skinned_mesh_component_t;


#ifdef __cplusplus
}
#endif // cpp