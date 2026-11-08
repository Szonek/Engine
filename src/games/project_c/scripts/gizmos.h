#pragma once
#include "game_components.h"

#include <gizmo_registry.h>

#include <functional>

namespace project_c
{
// The editor's gizmos of the game's components (see engine::GizmoRegistry), drawn in Edit mode too: an enemy spawner's
// area as a wire box, a player start as a sphere with an arrow for its facing, an enemy's sight cone and close range
// (coloured by its awareness while it plays) and the arc behind it the player executes it from (green), the
// player's shove reach (orange). Brighter on the
// selected game object. `execution_tuning` gives the player's execution tuning, read on every draw.
// Register after the game components (register_game_components).
void register_gizmos(engine::GizmoRegistry& gizmos, std::function<ExecutionTuning()> execution_tuning);
}  // namespace project_c
