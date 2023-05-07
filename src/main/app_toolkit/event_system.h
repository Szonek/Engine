#pragma once
#include "engine.h"

#include "event_types_defs.h"

#include <array>

namespace engine
{
class InputEventSystem
{
public:
    struct UpdateResult
    {
        bool pointer_moved_event = false;
        bool pointer_clicked_event = false;
        PointerEventData event_data = {};
    };

    InputEventSystem(engine_application_t app_handle)
        : app_(app_handle)
    {
    }

    UpdateResult update();

    engine_coords_2d_t mouse_coords_prev_{};
    std::array<bool, ENGINE_MOUSE_BUTTON_COUNT> mouse_down_state_prev_{};

private:
    engine_application_t app_;
};

}
