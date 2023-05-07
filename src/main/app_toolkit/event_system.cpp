#include "event_system.h"
#include <iostream>


engine::InputEventSystem::UpdateResult engine::InputEventSystem::update()
{
    engine::InputEventSystem::UpdateResult ret{};

    const auto mouse_coords_current = engineApplicationGetMouseCoords(app_);
    std::array<bool, ENGINE_MOUSE_BUTTON_COUNT> mouse_down_state_current{};
    for (auto i = 0; i < ENGINE_MOUSE_BUTTON_COUNT; i++)
    {
        mouse_down_state_current[i] = engineApplicationIsMouseButtonDown(app_, static_cast<engine_mouse_button_t>(i));
    }

    if (mouse_coords_prev_.x != mouse_coords_current.x
        || mouse_coords_prev_.y != mouse_coords_current.y)
    {
        //std::cout << "Mouse moved! " << mouse_coords_current.x << ", "  << mouse_coords_current.y << std::endl;;
        ret.pointer_moved_event = true;
    }

    if (mouse_down_state_prev_ != mouse_down_state_current)
    {
        for (auto i = 0; i < mouse_down_state_current.size(); i++)
        {
            if (mouse_down_state_prev_[i] && !mouse_down_state_current[i])
            {
                ret.event_data.button = static_cast<engine_mouse_button_t>(i);
                ret.event_data.position[0] = mouse_coords_current.x;
                ret.event_data.position[1] = mouse_coords_current.y;
                //const auto str = fmt::format("Mouse button CLICKED: [{}], [{}, {}]\n", ret.event_data.button,
                //    ret.event_data.position[0], ret.event_data.position[1]);
                //engineLog(str.c_str());
                ret.pointer_clicked_event = true;
            }
        }
    }

    // cache results
    mouse_coords_prev_ = mouse_coords_current;
    mouse_down_state_prev_ = mouse_down_state_current;

    // return result;
    return ret;
}