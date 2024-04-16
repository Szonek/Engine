#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

#include <stdint.h>

typedef uint32_t engine_geometry_t;
typedef uint32_t engine_skin_t;
typedef struct _engine_mesh_component_t
{
	engine_geometry_t geometry;
    engine_skin_t skin;
    bool disable;
} engine_mesh_component_t;

typedef struct _engine_bone_component_t
{
    float inverse_bind_matrix[16];
} engine_bone_component_t;

typedef uint32_t engine_game_object_t;
typedef struct _engine_skinned_mesh_component_t
{
    engine_geometry_t geometry;
    engine_game_object_t root_bone;
    bool disable;
} engine_skinned_mesh_component_t;

#ifdef __cplusplus
}
#endif // cpp