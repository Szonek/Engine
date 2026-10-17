#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

#include <stdint.h>

typedef uint32_t engine_geometry_t;
typedef uint32_t engine_game_object_t;

typedef struct _engine_mesh_component_t
{
	engine_geometry_t geometry;
    bool disable;
} engine_mesh_component_t;

typedef struct _engine_skinned_mesh_component_t
{
    engine_geometry_t geometry;
    // the game object whose skin pose deforms this mesh (the one with the skin component)
    engine_game_object_t skin;
    bool disable;
} engine_skinned_mesh_component_t;


#ifdef __cplusplus
}
#endif // cpp