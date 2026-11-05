#pragma once
#include "engine.h"
#include "utils.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace engine
{
class IScene;

// Gizmos: the shapes the editor shows for what a component does, such as an enemy spawner's area or the facing of a
// player start. A drawer is registered per component type, like Unreal's component visualizers or Unity's [DrawGizmo]
// static methods, not per script: scripts exist only while playing, a drawer needs no running instance, so gizmos show
// in Edit mode too, also on game objects without a script. Every scene calls the drawers each frame while the editor
// shows gizmos (engineEditorAreGizmosVisible), in every play state, before the engine's scene update draws them.
// A drawer only draws (engineDebugDraw*), it never changes the scene. What it draws is owned by its game object
// (engineDebugDrawSetOwner), so clicking it in the editor selects the game object.
class ENGINE_APP_TOOLKIT_API GizmoRegistry
{
public:
    // `scene`: the scene drawing it (to read a script's state while playing, e.g. get_script); `selected`: the game
    // object is the one selected in the editor (draw it brighter, or with more detail)
    using Drawer = std::function<void(const IScene& scene, engine_game_object_t go, bool selected)>;

    // the drawer of a game-defined component type (engineComponentTypeRegister); a type may have several
    void add(engine_component_type_t component_type, Drawer drawer);

    // calls each drawer for every game object of the active scene (`scene`) with its component; `selected` is the
    // editor's selection (ENGINE_INVALID_GAME_OBJECT_ID for none). Returns the number of drawer calls.
    std::size_t draw(const IScene& scene, engine_game_object_t selected) const;
    // counts the draw passes (draw calls), so a drawer can compute what all its game objects share once per pass
    // (e.g. read a data asset), not once per game object
    std::uint64_t pass() const { return passes_; }

private:
    // the drawers of each component type, so its game objects are listed once per frame
    struct Entry
    {
        engine_component_type_t component_type;
        std::vector<Drawer> drawers;
    };
    std::vector<Entry> types_;
    mutable std::uint64_t passes_ = 0;
};
}  // namespace engine
