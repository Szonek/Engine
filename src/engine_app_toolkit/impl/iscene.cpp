#include "iscene.h"
#include "iapplication.h"

#include "event_types_defs.h"

#include <fmt/format.h>

#include <iostream>
#include <map>

namespace
{
engine_result_code_t update_scene(engine_application_t app, engine_scene_t scene, float dt)
{
    auto engine_error_code = engineApplicationFrameSceneUpdate(app, scene, dt);
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
           scripts[col.object_a]->on_collision(collision);
        }
        else
        {
            //engineLog(fmt::format("Possible bug. Tried to send event to object without attached script, go id: {}\n", col.object_a).c_str());
        }

        collision.other = col.object_a;
        if (scripts.find(col.object_b) != scripts.end())
        {
            scripts[col.object_b]->on_collision(collision);
        }
        else
        {
            //engineLog(fmt::format("Possible bug. Tried to send event to object without attached script, go id: {}\n", col.object_b).c_str());
        }
    }
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t update_scripts(std::unordered_map<engine_game_object_t, std::unique_ptr<engine::IScript>>& scripts, float dt)
{
    for (auto& [go, script] : scripts)
    {
        script->update(dt);
    }
    return ENGINE_RESULT_CODE_OK;
}

inline engine_scene_t create_scene(engine_application_t app_handle)
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


engine::IScene::IScene(IApplication* app)
    : app_(app)
    , scene_(create_scene(get_app_handle()))
{
    if (!scene_)
    {
        throw std::runtime_error("Couldn't create scene!\n");
    }
    scripts_.reserve(1024);
}

engine::IScene::~IScene()
{
    // delete all scripts immediately before deallocating scene
    scripts_.clear();
    // delete scene
    if (scene_)
    {
        engineApplicationSceneDestroy(get_app_handle(), scene_);
    }
}

engine_application_t engine::IScene::get_app_handle()
{
    return app_->get_handle();
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

    propagate_collisions_events(get_app_handle(), scene_, scripts_);

    update_scripts(scripts_, dt);
    update_scene(get_app_handle(), scene_, dt);

    update_hook_end();

    for (auto& srq : scripts_unregister_queue_)
    {
        scripts_.erase(srq->get_game_object());
    }
    scripts_unregister_queue_.clear();

    return ENGINE_RESULT_CODE_OK;
}

