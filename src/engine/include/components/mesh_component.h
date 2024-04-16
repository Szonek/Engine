#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

#include <stdint.h>

typedef uint32_t engine_geometry_t;
typedef struct _engine_mesh_component_t
{
	engine_geometry_t geometry;
    bool disable;
} engine_mesh_component_t;

typedef struct _engine_bone_component_t
{
    float inverse_bind_matrix[16];
} engine_bone_component_t;

typedef uint32_t engine_game_object_t;
#define ENGINE_SKINNED_MESH_COMPONENT_MAX_SKELETON_BONES 64
typedef struct _engine_skin_component_t
{
    engine_game_object_t bones[ENGINE_SKINNED_MESH_COMPONENT_MAX_SKELETON_BONES];
} engine_skin_component_t;

#ifdef __cplusplus
}
#endif // cpp