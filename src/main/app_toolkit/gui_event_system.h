#pragma once
#include "engine.h"

#include <unordered_map>

namespace engine_systems
{
class GuiEventSystem
{
public:
    GuiEventSystem(engine_scene_t scene, std::unordered_map<engine_game_object_t, class IScript*>& scripts);
    ~GuiEventSystem();

    void update(float dt);

private:
    engine_component_view_t view_{};
};
}