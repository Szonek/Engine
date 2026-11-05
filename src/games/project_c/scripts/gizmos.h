#pragma once
#include <gizmo_registry.h>

namespace project_c
{
// The editor's gizmos of the game's components (see engine::GizmoRegistry), drawn in Edit mode too: an enemy spawner's
// area as a wire box, a player start as a sphere with an arrow for its facing, an enemy's sight cone and close range
// (coloured by its awareness while it plays). Brighter on the selected game object.
// Register after the game components (register_game_components).
void register_gizmos(engine::GizmoRegistry& gizmos);
}  // namespace project_c
