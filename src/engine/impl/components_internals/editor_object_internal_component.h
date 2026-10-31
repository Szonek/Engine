#pragma once

namespace engine
{
// Marks a game object the editor owns (its camera), like Unity's HideFlags.DontSave or Unreal's editor-only actors: it
// is transient (never saved) and clearing the scene keeps it (clear_game_objects), so it outlives the scene's content.
struct editor_object_internal_component_t
{
    // EnTT keeps no value for an empty type, which Scene::add_component returns
    bool reserved = false;
};
}  // namespace engine
