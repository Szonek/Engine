#include "iscene.h"
#include "iapplication.h"
#include "profiler.h"
#include "event_types_defs.h"

#include <fmt/format.h>

#include <iostream>
#include <map>

namespace
{
engine_result_code_t update_scene(float dt)
{
    auto engine_error_code = engineFrameSceneUpdate(dt);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Scene update failed. Exiting.\n"));
    }
    return engine_error_code;
}

engine_result_code_t propagate_collisions_events(engine::IScene::ScriptsMap& scripts)
{
    engine::ScopedProfiler prof("propagate_collisions_events");
    const auto num_collisions = enginePhysicsGetNumCollisions();
    for (std::size_t i = 0; i < num_collisions; i++)
    {
        const auto col_desc = enginePhysicsGetCollisionDesc(i);
        const auto obj_a = engineCollisionDescGetObjectA(col_desc);
        const auto obj_b = engineCollisionDescGetObjectB(col_desc);

        engine::IScript::collision_t collision{};
        collision.contact_points.resize(engineCollisionDescGetContactPointsCount(col_desc));
        for (std::size_t j = 0; j < collision.contact_points.size(); j++)
        {
            const auto cp = engineCollisionDescGetContactPoint(col_desc, j);
            collision.contact_points[j].lifetime = engineCollisionContactPointDescGetLifetime(cp);
            collision.contact_points[j].point_on_a = engineCollisionContactPointDescGetPointOnObjectA(cp);
            collision.contact_points[j].point_on_b = engineCollisionContactPointDescGetPointOnObjectB(cp);
        }

        collision.other = obj_b;
        if (scripts.find(obj_a) != scripts.end())
        {
            scripts[obj_a]->on_collision(collision);
        }
        else
        {
            //engineLog(fmt::format("Possible bug. Tried to send event to object without attached script, go id: {}\n", col.object_a).c_str());
        }

        collision.other = obj_a;
        if (scripts.find(obj_b) != scripts.end())
        {
            scripts[obj_b]->on_collision(collision);
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
    engine::ScopedProfiler prof("update_scripts");
    for (auto& [go, script] : scripts)
    {
        script->update(dt);
    }
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t late_update_scripts(std::unordered_map<engine_game_object_t, std::unique_ptr<engine::IScript>>& scripts, float dt)
{
    engine::ScopedProfiler prof("late_update_scripts");
    for (auto& [go, script] : scripts)
    {
        script->late_update(dt);
    }
    return ENGINE_RESULT_CODE_OK;
}

inline engine_scene_t create_scene()
{
    engine::ScopedProfiler prof("create_scene");
    engine_scene_t scene = nullptr;

    engine_scene_create_desc_t desc{};

    auto engine_error_code = engineSceneCreate(desc, &scene);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        engineSceneDestroy(scene);
        scene = nullptr;
    }
    return scene;
}

}  // namespace


engine::IScene::IScene(IApplication* app)
    : app_(app)
    , scene_(create_scene())
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
        engineSceneDestroy(scene_);
    }
}

engine_application_t engine::IScene::get_app_handle()
{
    return app_->get_handle();
}

bool engine::IScene::is_active() const
{
    return engineGetActiveScene() == scene_;
}

void engine::IScene::set_active()
{
    engineSetActiveScene(scene_);
}

engine_result_code_t engine::IScene::update(float dt)
{
    engine::ScopedProfiler prof("engine::IScene::update");
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

    propagate_collisions_events(scripts_);

    update_scripts(scripts_, dt);
    late_update_scripts(scripts_, dt);
    update_scene(dt);

    update_hook_end();

    for (auto& srq : scripts_unregister_queue_)
    {
        scripts_.erase(srq->get_game_object());
    }
    scripts_unregister_queue_.clear();

    return ENGINE_RESULT_CODE_OK;
}

