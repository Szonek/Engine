#pragma once

namespace engine
{
// Whether the game's gizmo drawers run this frame (engineEditorAreGizmosVisible): each view has its own toggle, like
// Unity's Scene view and Game view Gizmos. `editor_view` is the editor's free camera view, otherwise the game camera's
// is shown.
inline bool are_gizmos_visible(bool editor_view, bool editor_view_gizmos, bool game_view_gizmos)
{
    return editor_view ? editor_view_gizmos : game_view_gizmos;
}
}  // namespace engine
