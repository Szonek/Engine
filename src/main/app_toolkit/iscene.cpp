#include "iscene.h"

#include "event_types_defs.h"

#include <fmt/format.h>

#include <iostream>

namespace
{
engine_result_code_t update_physics(engine_application_t app, engine_scene_t scene, float dt)
{
    auto engine_error_code = engineApplicationFrameSceneUpdatePhysics(app, scene, dt);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Scene physcis update failed. Exiting.\n"));
    }
    return engine_error_code;
}

engine_result_code_t update_graphics(engine_application_t app, engine_scene_t scene, float dt)
{
    auto engine_error_code = engineApplicationFrameSceneUpdateGraphics(app, scene, dt);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Scene update failed. Exiting.\n"));
    }
    return engine_error_code;
}

engine_result_code_t propagate_collisions_events(engine_application_t app, engine_scene_t scene, engine::IScene::ScriptsMap& scripts)
{
    std::size_t num_collisions = 0;
    const engine_collision_info_t* collisions_list = nullptr;
    engineSceneGetCollisions(scene, &num_collisions, &collisions_list);
    for (std::size_t i = 0; i < num_collisions; i++)
    {
        const auto& col = collisions_list[i];
        engine::IScript::collision_t collision{};
        collision.contact_points.resize(col.contact_points_count);
        for (std::size_t j = 0; j < col.contact_points_count; j++)
        {
            collision.contact_points[j].lifetime = col.contact_points[j].lifetime;
            collision.contact_points[j].point[0] = col.contact_points[j].point_object_a[0];
            collision.contact_points[j].point[1] = col.contact_points[j].point_object_a[1];
            collision.contact_points[j].point[2] = col.contact_points[j].point_object_a[2];
        }

        collision.other = col.object_b;
        scripts[col.object_a]->on_collision(collision);

        collision.other = col.object_a;
        scripts[col.object_b]->on_collision(collision);
    }
    return ENGINE_RESULT_CODE_OK;
}

void propagte_input_events(engine_application_t app, engine_scene_t scene, const engine::InputEventSystem::UpdateResult& input_events, engine::IScene::ScriptsMap& scripts)
{
    const engine_finger_info_t* finger_infos = nullptr;
    std::size_t fingers_info_count = 0;
    const auto has_finger_info = engineApplicationGetFingerInfo(app, &finger_infos, &fingers_info_count);

    const auto mouse_coords = engineApplicationGetMouseCoords(app);
    //engineApplicationMosee

    engine_component_view_t rect_tranform_view{};
    engineCreateComponentView(&rect_tranform_view);
    engineSceneComponentViewAttachRectTransformComponent(scene, rect_tranform_view);

    engine_component_iterator_t begin_it{};
    engineComponentViewCreateBeginComponentIterator(rect_tranform_view, &begin_it);
    engine_component_iterator_t end_it{};
    engineComponentViewCreateEndComponentIterator(rect_tranform_view, &end_it);

    while (engineComponentIteratorCheckEqual(begin_it, end_it) == false)
    {
        const auto game_obj = engineComponentIteratorGetGameObject(begin_it);
        const auto rect_transform = engineSceneGetRectTransformComponent(scene, game_obj);

        if (input_events.pointer_clicked_event)
        {
            //ToDo: fix rect transform and change the scale to width/height in below condition
            if (input_events.event_data.position[0] >= rect_transform.position[0]    //x0
                && input_events.event_data.position[1] >= rect_transform.position[1] //y0
                && input_events.event_data.position[0] <= rect_transform.scale[0]
                && input_events.event_data.position[1] <= rect_transform.scale[1])
            {
                //const auto name_comp = engineSceneGetNameComponent(scene, game_obj);
                //std::cout << "GO: " << game_obj <<", " << name_comp.name << std::endl;

                scripts[game_obj]->on_pointer_click(&input_events.event_data);
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

engine_result_code_t update_scripts(std::unordered_map<engine_game_object_t, std::unique_ptr<engine::IScript>>& scripts, float dt)
{
    for (auto& [go, script] : scripts)
    {
        script->update(dt);
    }
    return ENGINE_RESULT_CODE_OK;
}

}  // namespace


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


engine::IScene::IScene(engine_application_t app_handle, engine_result_code_t& engine_error_code)
    : app_(app_handle)
    , input_event_system_(app_handle)
{
    engine_error_code = engineSceneCreate(&scene_);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        engineSceneDestroy(scene_);
        log(fmt::format("Couldnt create scene!\n"));
        return;
    }
    scripts_.reserve(1024);
}

engine::IScene::~IScene()
{
    if (scene_)
    {
        engineSceneDestroy(scene_);
    }
}

void engine::IScene::activate()
{
    is_activate_ = true;
}

void engine::IScene::deactivate()
{
    is_activate_ = false;
}

bool engine::IScene::is_active() const
{
    return is_activate_;
}

engine_result_code_t engine::IScene::update(float dt)
{
    if (!is_active())
    {
        return ENGINE_RESULT_CODE_OK;
    }

    //
    const auto input_events = input_event_system_.update();
    propagte_input_events(app_, scene_, input_events, scripts_);
    propagate_collisions_events(app_, scene_, scripts_);

    update_physics(app_, scene_, dt);


    update_scripts(scripts_, dt);

    update_graphics(app_, scene_, dt);

    return ENGINE_RESULT_CODE_OK;
}

