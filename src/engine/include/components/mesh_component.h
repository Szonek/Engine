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

#ifdef __cplusplus
}
#endif // cpp