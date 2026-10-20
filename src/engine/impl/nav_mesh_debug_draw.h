#pragma once
#include "render/render_scene_data.h"

#include <vector>

namespace engine
{
class NavMesh;
// the nav mesh's polygon outlines (Recast's DebugUtils) as debug lines
void collect_nav_mesh_debug_lines(const NavMesh& nav_mesh, std::vector<DebugLineGpuData>& out_lines);
}  // namespace engine
