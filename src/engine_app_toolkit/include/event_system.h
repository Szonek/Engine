#pragma once
#include "engine.h"
#include "utils.h"
#include "event_types_defs.h"

#include <vector>
#include <array>

namespace engine
{
class ENGINE_APP_TOOLKIT_API InputEventSystem
{
public:
    struct UpdateResult
    {
        bool pointer_moved_event = false;
        bool pointer_clicked_event = false;
        bool pointer_down_event = false;
        PointerEventData event_data = {};
    };

    InputEventSystem(engine_application_t& app_handle, engine_scene_t& scene)
        : app_(app_handle)
        , scene_(scene)
    {
    }

    std::vector<UpdateResult> update();

private:
    engine_application_t& app_;
    engine_scene_t& scene_;

    // mouse
    engine_coords_2d_t mouse_coords_prev_{};
    std::array<bool, ENGINE_MOUSE_BUTTON_COUNT> mouse_down_state_prev_{};

    // touch 
    engine_fingers_infos_list_t touch_info_prev_{};
};

}
