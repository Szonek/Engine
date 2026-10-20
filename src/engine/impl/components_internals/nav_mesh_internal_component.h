#pragma once
#include "../nav_mesh.h"

#include <memory>

namespace engine
{
// The scene's nav mesh, on the game object with the nav surface component: built by engineNavMeshBuild(), destroyed with
// that component or the game object. Derived data, never saved.
struct nav_mesh_internal_component_t
{
    std::unique_ptr<NavMesh> nav_mesh;
};
}  // namespace engine
