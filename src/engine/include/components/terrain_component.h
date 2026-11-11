#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp

#include <stdint.h>

typedef uint32_t engine_terrain_t;

// Draws a heightmap terrain (like Unity's Terrain): the ground of outdoor areas. The heights are a terrain asset
// (engineTerrainLoad / engineTerrainCreate), shared by every component referencing it. The terrain starts at its game
// object's origin and extends along its +x and +z axes; its transform places it.
//
// It is drawn with the game object's material component (its colour, texture and shininess), like a mesh, so a terrain
// without a material isn't drawn. Collision is separate: a collider of ENGINE_COLLIDER_TYPE_HEIGHTFIELD referencing the
// same terrain asset, which is also what nav mesh builds read.
typedef struct _engine_terrain_component_t
{
    engine_terrain_t terrain;
    // how many metres one repeat of the material's textures covers (the terrain's texture coordinates come from its
    // local x and z); 0 or less stretches them once over the whole terrain
    float texture_tile_size;
    bool disable;
} engine_terrain_component_t;

#ifdef __cplusplus
}
#endif // cpp
