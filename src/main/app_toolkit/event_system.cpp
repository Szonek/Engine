#include "event_system.h"
#include "engine.h"
#include "utils.h"


std::vector<engine::InputEventSystem::UpdateResult> engine::InputEventSystem::update()
{
    using UR = engine::InputEventSystem::UpdateResult;
    std::vector<UR> ur_candidates{};  // candidates to return, since we dont know if game object was found
    ur_candidates.reserve(64);

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
                ur_candidates.push_back(r);
            }
            else if(touch_info_current.infos[i].event_type_flags & ENGINE_FINGER_DOWN || touch_info_current.infos[i].event_type_flags & ENGINE_FINGER_MOTION)
            {
                r.event_data.button = ENGINE_MOUSE_BUTTON_LEFT;
                r.pointer_down_event = true;
                ur_candidates.push_back(r);
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

    std::vector<UR> ret{};  // candidates to return, since we dont know if game object was found
    ret.reserve(64);
    if (!ur_candidates.empty())
    {
        engine_component_view_t rect_tranform_view{};
        engineCreateComponentView(&rect_tranform_view);
        engineSceneComponentViewAttachRectTransformComponent(scene_, rect_tranform_view);

        engine_component_iterator_t begin_it{};
        engineComponentViewCreateBeginComponentIterator(rect_tranform_view, &begin_it);
        engine_component_iterator_t end_it{};
        engineComponentViewCreateEndComponentIterator(rect_tranform_view, &end_it);

        while (engineComponentIteratorCheckEqual(begin_it, end_it) == false)
        {
            const auto game_obj = engineComponentIteratorGetGameObject(begin_it);
            const auto rect_transform = engineSceneGetRectTransformComponent(scene_, game_obj);

            for (auto& input_event : ur_candidates)
            {
                //ToDo: fix rect transform and change the scale to width/height in below condition
                const bool position_within_rect_transform_bounds = input_event.event_data.position[0] >= rect_transform.position[0]    //x0
                                                                   && input_event.event_data.position[1] >= rect_transform.position[1] //y0
                                                                   && input_event.event_data.position[0] <= rect_transform.scale[0]
                                                                   && input_event.event_data.position[1] <= rect_transform.scale[1];
                // click event has to be finished within object
                if (input_event.pointer_clicked_event && position_within_rect_transform_bounds)
                {
                    input_event.event_data.pointer_click_object = game_obj;
                    ret.push_back(input_event);
                }

                // down event - pointer is clicked (not released yet) on the object
                if (input_event.pointer_down_event && position_within_rect_transform_bounds)
                {
                    input_event.event_data.pointer_down_object = game_obj;
                    ret.push_back(input_event);
                }
            }
            engineComponentIteratorNext(begin_it);
        }

        if (begin_it)
        {
            engineDeleteComponentIterator(begin_it);
        }

        if (end_it)
        {
            engineDeleteComponentIterator(end_it);
        }

        if (rect_tranform_view)
        {
            engineDestroyComponentView(rect_tranform_view);
        }

    }
    return ret;
}