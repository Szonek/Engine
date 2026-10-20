#include "iscene.h"
#include "iapplication.h"
#include "profiler.h"
#include "event_types_defs.h"

#include <fmt/format.h>

#include <iostream>
#include <map>

namespace
{

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
    if (!app)
    {
        throw std::runtime_error("Invalid app handle!\n");
    }
    if (!scene_)
    {
        throw std::runtime_error("Couldn't create scene!\n");
    }
    activate();
    scripts_.reserve(1024);
}

engine::IScene::~IScene()
{
    // delete all scripts immediately before deallocating scene
    scripts_.clear();
    // scripts registered since the last update are owned by the register queue until it is flushed
    for (auto* script : scripts_register_queue_)
    {
        delete script;
    }
    scripts_register_queue_.clear();
    // the unregister queue doesn't own its scripts: they were in scripts_ or in the register queue
    scripts_unregister_queue_.clear();
    // delete scene
    if (scene_)
    {
        engineSceneDestroy(scene_);
    }
}

void engine::IScene::activate()
{
    engineSceneSetActive(scene_);
    is_activate_ = true;
}

void engine::IScene::deactivate()
{
    is_activate_ = false;
    engineSceneSetActive(nullptr);
}

bool engine::IScene::is_active() const
{
    return is_activate_;
}

void engine::IScene::flush_register_queue(bool playing)
{
    engine::ScopedProfiler prof("engine::IScene::flush_register_queue");
    do
    {
        // on_start can register more scripts, so take this batch out of the queue first
        ScriptsQueue batch;
        batch.swap(scripts_register_queue_);
        for (auto* script : batch)
        {
            const auto go = script->get_game_object();
            // its game object was destroyed while it waited in the queue: it never starts nor updates
            if (!engineGameObjectIsValid(go))
            {
                std::erase(scripts_unregister_queue_, script);
                delete script;
                continue;
            }
            scripts_[go] = std::unique_ptr<IScript>(script);
            scripts_to_start_.push_back(go);
        }

        if (!playing)
        {
            return;
        }
        std::vector<engine_game_object_t> to_start;
        to_start.swap(scripts_to_start_);
        for (const auto go : to_start)
        {
            // an earlier on_start may have destroyed it
            const auto it = scripts_.find(go);
            if (it != scripts_.end() && engineGameObjectIsValid(go))
            {
                it->second->on_start();
            }
        }
    } while (!scripts_register_queue_.empty());
}

engine_result_code_t engine::IScene::update(float dt)
{
    engine::ScopedProfiler prof("engine::IScene::update");
    if (!is_active())
    {
        return ENGINE_RESULT_CODE_OK;
    }

    const bool gameplay_paused = !engineEditorIsPlaying();
    flush_register_queue(!gameplay_paused);

    if (!gameplay_paused)
    {
        update_hook_begin();
        propagate_collisions_events(scripts_);
        update_scripts(scripts_, dt);
        late_update_scripts(scripts_, dt);
    }

    if (engineFrameSceneUpdate(gameplay_paused ? 0.0f : dt) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Scene update failed. Exiting.\n"));
        return ENGINE_RESULT_CODE_FAIL;
    }
    
    // editor could destroy game object (engine shouldn't do it), clean up scripts
    for (auto& [go, script] : scripts_)
    {
        if (!engineGameObjectIsValid(go))
        {
            unregister_script(script.get());
        }
    }

    if (!gameplay_paused)
    {
        update_hook_end();
    }

    for (auto& srq : scripts_unregister_queue_)
    {
        scripts_.erase(srq->get_game_object());
    }
    scripts_unregister_queue_.clear();

    return ENGINE_RESULT_CODE_OK;
}
