#include "iscene.h"
#include "iapplication.h"
#include "profiler.h"
#include "event_types_defs.h"

#include <fmt/format.h>

#include <algorithm>
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

        // a game object destroyed since the physics step (its script goes at the end of this update) gets no events
        collision.other = obj_b;
        if (engineGameObjectIsValid(obj_a) && scripts.find(obj_a) != scripts.end())
        {
            scripts[obj_a]->on_collision(collision);
        }
        else
        {
            //engineLog(fmt::format("Possible bug. Tried to send event to object without attached script, go id: {}\n", col.object_a).c_str());
        }

        collision.other = obj_a;
        if (engineGameObjectIsValid(obj_b) && scripts.find(obj_b) != scripts.end())
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

engine::IScript* engine::IScene::attach_script(engine_game_object_t go, const std::type_info& type)
{
    auto& registry = app_->scripts();
    const auto index = registry.find_index(type);
    const auto component_type = registry.get_component_type();
    if (index == ScriptRegistry::NO_SCRIPT || component_type == ENGINE_INVALID_OBJECT_HANDLE || !engineGameObjectIsValid(go) || has_script(go))
    {
        log(fmt::format("Can't attach script {} to game object {}: it isn't registered, the script component isn't, or the game object "
            "has a script.\n", type.name(), go));
        return nullptr;
    }
    const ScriptComponent component{ index };
    if (!engineGameObjectHasDataComponent(go, component_type))
    {
        engineGameObjectAddDataComponent(go, component_type, &component);
    }
    else
    {
        engineGameObjectUpdateDataComponent(go, component_type, &component);
    }
    // no scripts in Edit mode (nor while a Stop ends the session): the component is data, the script is created when a
    // play session starts
    if (!is_play_session_state())
    {
        add_required_components(go, index);
        return nullptr;
    }
    return create_script_from_component(go, index);
}

void engine::IScene::add_required_components(engine_game_object_t go, std::int32_t index)
{
    if (const auto* script_type = app_->scripts().find(index))
    {
        for (const auto required : script_type->required)
        {
            if (!engineGameObjectHasDataComponent(go, required))
            {
                engineGameObjectAddDataComponent(go, required, nullptr);
            }
        }
    }
}

engine::IScript* engine::IScene::create_script_from_component(engine_game_object_t go, std::int32_t index)
{
    const auto* script_type = app_->scripts().find(index);
    if (!script_type)
    {
        return nullptr;
    }
    add_required_components(go, index);
    auto* script = register_script(script_type->factory(this, go));
    component_scripts_[go] = index;
    return script;
}

bool engine::IScene::has_script(engine_game_object_t go) const
{
    return scripts_.contains(go)
        || std::ranges::any_of(scripts_register_queue_, [go](const IScript* s) { return s->get_game_object() == go; });
}

void engine::IScene::remove_script_keep_game_object(engine_game_object_t go)
{
    component_scripts_.erase(go);
    // a script which hasn't started yet: its replacement is queued to start once
    std::erase(scripts_to_start_, go);
    // at once, not through the unregister queue: a new script for the same game object may be registered right after
    if (const auto it = scripts_.find(go); it != scripts_.end())
    {
        it->second->destroys_game_object_ = false;
        std::erase(scripts_unregister_queue_, it->second.get());
        scripts_.erase(it);
        return;
    }
    const auto queued = std::ranges::find_if(scripts_register_queue_, [go](const IScript* s) { return s->get_game_object() == go; });
    if (queued != scripts_register_queue_.end())
    {
        auto* script = *queued;
        scripts_register_queue_.erase(queued);
        std::erase(scripts_unregister_queue_, script);
        script->destroys_game_object_ = false;
        delete script;
    }
}

void engine::IScene::sync_scripts_with_components()
{
    engine::ScopedProfiler prof("engine::IScene::sync_scripts_with_components");
    const auto component_type = app_->scripts().get_component_type();
    if (component_type == ENGINE_INVALID_OBJECT_HANDLE)
    {
        return;
    }
    const auto changes = engineDataComponentGetChangeCount(component_type);
    if (changes == synced_script_changes_)
    {
        return;
    }

    // the game objects with a script component and the script each names
    std::vector<engine_game_object_t> with_script;
    engine_component_view_t view = nullptr;
    engineComponentViewCreate(&view);
    engineComponentViewAttachDataComponent(view, component_type);
    engine_component_iterator_t it = nullptr;
    engine_component_iterator_t end = nullptr;
    engineComponentViewCreateBeginComponentIterator(view, &it);
    engineComponentViewCreateEndComponentIterator(view, &end);
    for (; !engineComponentIteratorCheckEqual(it, end); engineComponentIteratorNext(it))
    {
        with_script.push_back(engineComponentIteratorGetGameObject(it));
    }
    engineComponentIteratorDelete(it);
    engineComponentIteratorDelete(end);
    engineComponentViewDestroy(view);
    std::vector<std::pair<engine_game_object_t, std::int32_t>> wanted;
    for (const auto go : with_script)
    {
        ScriptComponent component{};
        engineGameObjectGetDataComponent(go, component_type, &component);
        wanted.emplace_back(go, component.type);
    }

    // scripts whose component was removed, or whose game object is gone
    std::vector<engine_game_object_t> removed;
    for (const auto& [go, index] : component_scripts_)
    {
        if (!std::ranges::any_of(wanted, [go](const auto& w) { return w.first == go; }))
        {
            removed.push_back(go);
        }
    }
    for (const auto go : removed)
    {
        remove_script_keep_game_object(go);
    }
    for (const auto& [go, index] : wanted)
    {
        const auto known = component_scripts_.find(go);
        if (known != component_scripts_.end() && known->second == index)
        {
            continue;
        }
        if (known != component_scripts_.end())
        {
            remove_script_keep_game_object(go);
        }
        else if (has_script(go))
        {
            log(fmt::format("Game object {} has a script registered by code, its script component is ignored.\n", go));
            continue;
        }
        if (index != ScriptRegistry::NO_SCRIPT && !create_script_from_component(go, index))
        {
            log(fmt::format("Game object {}: its script component names no registered script ({}).\n", go, index));
        }
    }
    // the components added by the scripts' requirements and attach_script changed nothing to sync
    synced_script_changes_ = engineDataComponentGetChangeCount(component_type);
}

void engine::IScene::begin_play_session()
{
    on_play_begin();
    // every script component gets its script, whatever the change count says
    synced_script_changes_.reset();
}

void engine::IScene::end_play_session()
{
    // the game objects stay: they are the play session's, which Stop throws away at the end of this frame, loading the
    // scene saved at Play back. Every game object still exists, so the scripts' destructors clean up as always.
    for (auto& [go, script] : scripts_)
    {
        script->destroys_game_object_ = false;
    }
    scripts_.clear();
    for (auto* script : scripts_register_queue_)
    {
        script->destroys_game_object_ = false;
        delete script;
    }
    scripts_register_queue_.clear();
    scripts_unregister_queue_.clear();
    scripts_to_start_.clear();
    component_scripts_.clear();
    synced_script_changes_.reset();
    on_play_end();
}

void engine::IScene::drop_scripts_of_destroyed_game_objects()
{
    // a dropped script's destructor may destroy other game objects with scripts (its runtime parts): until none is left
    for (;;)
    {
        const auto it = std::ranges::find_if(scripts_, [](const auto& entry) { return !engineGameObjectIsValid(entry.first); });
        if (it == scripts_.end())
        {
            return;
        }
        const auto go = it->first;
        auto script = std::move(it->second);
        scripts_.erase(it);
        component_scripts_.erase(go);
        std::erase(scripts_to_start_, go);
        std::erase(scripts_unregister_queue_, script.get());
        // out of the map before its destructor runs
        script.reset();
    }
}

engine_result_code_t engine::IScene::update(float dt)
{
    engine::ScopedProfiler prof("engine::IScene::update");
    if (!is_active())
    {
        return ENGINE_RESULT_CODE_OK;
    }

    // a Stop ends this scene's session in its last frame, while the scene is still there; Play (or an application which
    // starts playing) begins one
    const auto changes = play_session_.update();
    if (changes.ended)
    {
        end_play_session();
    }
    if (changes.began)
    {
        begin_play_session();
    }
    const bool gameplay_paused = !is_play_session_state() || !engineEditorIsPlaying();
    // in Edit mode no scripts exist
    if (play_session_.in_session())
    {
        // destroyed since the last update (by the game outside of it): nothing ticks on a dead game object
        drop_scripts_of_destroyed_game_objects();
        sync_scripts_with_components();
        flush_register_queue(!gameplay_paused);
    }

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
    
    // destroyed during this update (by a script, or the editor's delete)
    drop_scripts_of_destroyed_game_objects();

    if (!gameplay_paused)
    {
        update_hook_end();
    }

    for (auto& srq : scripts_unregister_queue_)
    {
        component_scripts_.erase(srq->get_game_object());
        scripts_.erase(srq->get_game_object());
    }
    scripts_unregister_queue_.clear();

    return ENGINE_RESULT_CODE_OK;
}
