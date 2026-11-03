#include "gizmo_registry.h"
#include "profiler.h"

#include <algorithm>
#include <utility>

void engine::GizmoRegistry::add(engine_component_type_t component_type, Drawer drawer)
{
    const auto it = std::ranges::find(types_, component_type, &Entry::component_type);
    if (it != types_.end())
    {
        it->drawers.push_back(std::move(drawer));
        return;
    }
    types_.push_back(Entry{ component_type, { std::move(drawer) } });
}

std::size_t engine::GizmoRegistry::draw(const IScene& scene, engine_game_object_t selected) const
{
    engine::ScopedProfiler prof("engine::GizmoRegistry::draw");
    std::size_t calls = 0;
    for (const auto& entry : types_)
    {
        for (const auto go : get_game_objects_with_data_component(entry.component_type))
        {
            // a click on what it draws selects the game object
            engineDebugDrawSetOwner(go);
            for (const auto& drawer : entry.drawers)
            {
                drawer(scene, go, go == selected);
                calls++;
            }
            engineDebugDrawSetOwner(ENGINE_INVALID_GAME_OBJECT_ID);
        }
    }
    return calls;
}
