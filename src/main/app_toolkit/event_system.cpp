#include "event_system.h"
#include "engine.h"
#include "utils.h"


std::vector<engine::InputEventSystem::UpdateResult> engine::InputEventSystem::update()
{
    using UR = engine::InputEventSystem::UpdateResult;
    std::vector<UR> ret{};
    ret.reserve(64);

    if(K_IS_ANDROID)
    {
        engine_fingers_infos_list_t touch_info_current{};
        const auto has_finger_info = engineApplicationGetFingerInfo(app_, &touch_info_current);
        for (std::size_t i = 0; i < ENGINE_FINGERS_INFOS_LIST_COUNT; i++)
        {
            // skip finger UNKNOWN event (no event at all)
            if (touch_info_current.infos[i].event_type_flags == ENGINE_FINGER_UNKNOWN)
            {
                continue;
            }
            UR r{};
            if (touch_info_prev_.infos[i].event_type_flags & ENGINE_FINGER_DOWN || touch_info_prev_.infos[i].event_type_flags & ENGINE_FINGER_MOTION)
            {
                r.event_data.position[0] = touch_info_current.infos[i].x;
                r.event_data.position[1] = touch_info_current.infos[i].y;
            }
            if(touch_info_current.infos[i].event_type_flags & ENGINE_FINGER_UP)
            {
                r.event_data.button = ENGINE_MOUSE_BUTTON_LEFT;
                r.pointer_clicked_event = true;
                ret.push_back(r);
            }
            else if(touch_info_current.infos[i].event_type_flags & ENGINE_FINGER_DOWN || touch_info_current.infos[i].event_type_flags & ENGINE_FINGER_MOTION)
            {
                if(r.event_data.position[0] == 0.0f && r.event_data.position[1] == 0.0f)
                {
                    engineLog("debug\n");
                }
                const auto s = fmt::format("{}, {}\n", r.event_data.position[0], r.event_data.position[1]);
                engineLog(s.c_str());
                r.event_data.button = ENGINE_MOUSE_BUTTON_LEFT;
                r.pointer_down_event = true;
                ret.push_back(r);
            }
            touch_info_prev_.infos[i] = touch_info_current.infos[i];
        }
    }
    else
    {
        //ToDoL implmenet mouse windows events
        //const auto mouse_coords_current = engineApplicationGetMouseCoords(app_);
        //std::array<bool, ENGINE_MOUSE_BUTTON_COUNT> mouse_down_state_current{};
        //for (auto i = 0; i < ENGINE_MOUSE_BUTTON_COUNT; i++)
        //{
        //    mouse_down_state_current[i] = engineApplicationIsMouseButtonDown(app_, static_cast<engine_mouse_button_t>(i));
        //}

        //if (mouse_coords_prev_.x != mouse_coords_current.x
        //    || mouse_coords_prev_.y != mouse_coords_current.y)
        //{
        //    //std::cout << "Mouse moved! " << mouse_coords_current.x << ", "  << mouse_coords_current.y << std::endl;;
        //    ret.pointer_moved_event = true;
        //}

        //if (mouse_down_state_prev_ != mouse_down_state_current)
        //{
        //    for (auto i = 0; i < mouse_down_state_current.size(); i++)
        //    {
        //        if (mouse_down_state_prev_[i] && !mouse_down_state_current[i])
        //        {
        //            ret.event_data.button = static_cast<engine_mouse_button_t>(i);
        //            ret.event_data.position[0] = mouse_coords_current.x;
        //            ret.event_data.position[1] = mouse_coords_current.y;
        //            //const auto str = fmt::format("Mouse button CLICKED: [{}], [{}, {}]\n", ret.event_data.button,
        //            //    ret.event_data.position[0], ret.event_data.position[1]);
        //            //engineLog(str.c_str());
        //            ret.pointer_clicked_event = true;
        //        }
        //    }
        //}

        //// cache results
        //mouse_coords_prev_ = mouse_coords_current;
        //mouse_down_state_prev_ = mouse_down_state_current;
    }

    // return result;
    return ret;
}