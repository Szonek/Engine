#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

// The navigation settings of a scene; a scene has one. engineNavMeshBuild() builds the scene's nav mesh with them from
// its static colliders (see engine_nav_modifier_component_t). The nav mesh is derived data: it is never saved, and a
// change of these settings takes effect with the next engineNavMeshBuild().
typedef struct _engine_nav_surface_component_t
{
    // the agents walking the nav mesh: their radius keeps them that far from walls and edges, they fit under anything
    // higher than their height, walk up slopes up to max slope (degrees) and step up to max climb (metres)
    float agent_radius;
    float agent_height;
    float agent_max_slope;
    float agent_max_climb;
    // size of the voxels the colliders are rasterized into (metres): smaller is more precise and slower to build
    float cell_size;
    float cell_height;
} engine_nav_surface_component_t;

#ifdef __cplusplus
}
#endif // cpp
