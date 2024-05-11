#include "iscene.h"
#include "scene_manager.h"

#include "event_types_defs.h"

#include <fmt/format.h>

#include <iostream>
#include <map>

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
    engineScenePhysicsGetCollisions(scene, &num_collisions, &collisions_list);

    static std::map<std::uint64_t, engine::IScript::collision_t> collision_cache{};

    for (std::size_t i = 0; i < num_collisions; i++)
    {
        const auto& col = collisions_list[i];
        //const std::uint64_t cache_key = (static_cast<std::uint64_t>(col.object_a) << 32) | col.object_b;
        //if (collision_cache.find(cache_key) != collision_cache.end())
        //{
        //    auto& collision = collision_cache[cache_key];
        //    collision.
        //    continue;
        //}
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
        if (scripts.find(col.object_a) != scripts.end())
        {
           scripts[col.object_a]->on_collision_enter(collision);
        }
        else
        {
            //engineLog(fmt::format("Possible bug. Tried to send event to object without attached script, go id: {}\n", col.object_a).c_str());
        }

        collision.other = col.object_a;
        if (scripts.find(col.object_b) != scripts.end())
        {
            scripts[col.object_b]->on_collision_enter(collision);
        }
        else
        {
            //engineLog(fmt::format("Possible bug. Tried to send event to object without attached script, go id: {}\n", col.object_b).c_str());
        }
    }
    return ENGINE_RESULT_CODE_OK;
}

void propagte_input_events(engine_application_t app, engine_scene_t scene, const std::vector<engine::InputEventSystem::UpdateResult>& input_events, engine::IScene::ScriptsMap& scripts)
{

    for (const auto& input_event : input_events)
    {
        if (input_event.pointer_clicked_event)
        {
            if (!scripts.contains(input_event.event_data.pointer_click_object))
            {
                engineLog(fmt::format("Bug!! Tried to send event to object without attached script, go id: {}\n", input_event.event_data.pointer_click_object).c_str());
            }
            else
            {
                //scripts[input_event.event_data.pointer_click_object]->on_pointer_click(&input_event.event_data);
            }
        }

        if (input_event.pointer_down_event)
        {
            if (!scripts.contains(input_event.event_data.pointer_down_object))
            {
                engineLog(fmt::format("Bug!! Tried to send event to object without attached script, go id: {}\n", input_event.event_data.pointer_down_object).c_str());
            }
            else
            {
                //scripts[input_event.event_data.pointer_down_object]->on_pointer_down(&input_event.event_data);
            }
        }
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

inline engine_scene_t create_scene(engine_application_t& app_handle)
{
    engine_scene_t scene = nullptr;

    engine_scene_create_desc_t desc{};

    auto engine_error_code = engineApplicationSceneCreate(app_handle, desc, &scene);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        engineApplicationSceneDestroy(app_handle, scene);
        scene = nullptr;
    }
    return scene;
}

}  // namespace


engine::IScene::IScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
    : app_(app_handle)
    , scene_(create_scene(app_handle))
    , scene_manager_(scn_mgn)
    , input_event_system_(app_, scene_)
{
    if (!scene_)
    {
        log(fmt::format("Couldn't create scene!\n"));
        return;
    }
    scripts_.reserve(1024);
    engine_error_code = ENGINE_RESULT_CODE_OK;
}

engine::IScene::~IScene()
{
    if (scene_)
    {
        engineApplicationSceneDestroy(app_, scene_);
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

    for (auto& srq : scripts_register_queue_)
    {
        scripts_[srq->get_game_object()] = std::unique_ptr<IScript>(srq);
    }
    scripts_register_queue_.clear();

    update_hook_begin();

    //
    const auto input_events = input_event_system_.update();
    //propagte_input_events(app_, scene_, input_events, scripts_);
    propagate_collisions_events(app_, scene_, scripts_);

    update_scripts(scripts_, dt);
    update_physics(app_, scene_, dt);
    update_graphics(app_, scene_, dt);

    update_hook_end();

    for (auto& srq : scripts_unregister_queue_)
    {
        scripts_.erase(srq->get_game_object());
    }
    scripts_unregister_queue_.clear();

    return ENGINE_RESULT_CODE_OK;
}

