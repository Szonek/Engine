#include "gizmo_registry.h"
#include "profiler.h"

#include <utility>

void engine::GizmoRegistry::add(engine_component_type_t component_type, Drawer drawer)
{
    drawers_.push_back(Entry{ component_type, std::move(drawer) });
}

std::size_t engine::GizmoRegistry::draw(engine_game_object_t selected) const
{
    engine::ScopedProfiler prof("engine::GizmoRegistry::draw");
    std::size_t calls = 0;
    for (const auto& entry : drawers_)
    {
        for (const auto go : get_game_objects_with_data_component(entry.component_type))
        {
            entry.drawer(go, go == selected);
            calls++;
        }
    }
    return calls;
}
