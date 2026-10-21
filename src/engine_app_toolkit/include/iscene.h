#pragma once
#include "engine.h"
#include "iscript.h"
#include "utils.h"

#include <unordered_map>
#include <deque>
#include <functional>
#include <memory>

namespace engine
{
class SceneManager;

class ENGINE_APP_TOOLKIT_API UserEventSystem
{
public:
    void register_event_callback(std::uint32_t ev_id, std::function<void()>&& callback)
    {
        callbacks_[ev_id].push_back(std::move(callback));
    }

    void activate_event(std::uint32_t ev_id)
    {
        for (const auto& listener : callbacks_[ev_id])
        {
            listener();
        }
    }


private:
    std::unordered_map<std::uint32_t, std::vector<std::function<void()>>> callbacks_;
};

class IApplication;

// Owns one engine scene and the scripts of its game objects (one script per game object).
//
// Script lifecycle (Unity Awake/Start, Godot _init/_ready, Unreal constructor/BeginPlay):
// 1. constructor: register_script<T>(...) creates the script, which sets up only its own game object. It waits in the
//    register queue until the next update; until then its creator may configure it (setters, its own game object).
// 2. on_start: at the start of update, all queued scripts are added to the scene first (so scripts registered in the
//    same frame can find each other), then each gets on_start, in registration order. Scripts registered in on_start
//    start in the same frame, before any update. While gameplay is paused (editor) scripts are added but start only
//    once it plays; a script whose game object was destroyed before it started never starts.
// 3. update, late_update, on_collision: every playing frame, once started.
// The load order this enables: create all game objects and components -> fix references between them -> create the
// scripts -> on_start for all of them.
class ENGINE_APP_TOOLKIT_API IScene
{
public:
    using ScriptsMap = std::unordered_map<engine_game_object_t, std::unique_ptr<IScript>>;
    using ScriptsQueue = std::deque<IScript*>;

public:
    IScene(IApplication* app);
    IScene(const IScene& rhs) = delete;
    IScene(IScene&& rhs) noexcept = default;
    IScene& operator=(const IScene& rhs) = delete;
    IScene& operator=(IScene&& rhs)  noexcept = default;
    virtual ~IScene();

    template<typename T,typename... TArgs>
    T* register_script(TArgs&&... args)
    {
        scripts_register_queue_.push_back(new T(this, args...));
        return (T*)scripts_register_queue_.back();
    }

    template<typename T>
    T* register_script(T* t)
    {
        scripts_register_queue_.push_back(t);
        return (T*)scripts_register_queue_.back();
    }

    template<typename T>
    void unregister_script(T* script)
    {
        assert(script);
        scripts_unregister_queue_.push_front(script);
    }

    template<typename T>
    T* get_script(engine_game_object_t go)
    {
        // nullptr when the game object has no script (or a script of different type)
        const auto it = scripts_.find(go);
        return it != scripts_.end() ? dynamic_cast<T*>(it->second.get()) : nullptr;
    }

    template<typename T>
    const T* get_script(engine_game_object_t go) const
    {
        const auto it = scripts_.find(go);
        return it != scripts_.end() ? dynamic_cast<const T*>(it->second.get()) : nullptr;
    }

    // every script of type T (or derived from it), in no particular order; scripts still waiting in the
    // register queue are not included
    template<typename T>
    std::vector<T*> get_scripts()
    {
        std::vector<T*> result;
        for (auto& [go, script] : scripts_)
        {
            if (auto* typed = dynamic_cast<T*>(script.get()))
            {
                result.push_back(typed);
            }
        }
        return result;
    }

    // the first script of type T, nullptr when the scene has none; for scripts that exist once per scene
    template<typename T>
    T* find_script()
    {
        for (auto& [go, script] : scripts_)
        {
            if (auto* typed = dynamic_cast<T*>(script.get()))
            {
                return typed;
            }
        }
        return nullptr;
    }

    IApplication* get_app() { return app_; }

    virtual void activate();
    virtual void deactivate();
    virtual bool is_active() const;
    virtual engine_result_code_t update(float dt);
    
    UserEventSystem* get_user_event_sysmte() { return &user_event_system_; }

    virtual void register_event_callback(std::uint32_t event_id, std::function<void()>&& callback)
    {
        user_event_system_.register_event_callback(event_id, std::move(callback));
    }

protected:
    virtual void update_hook_begin() {}
    virtual void update_hook_end() {}

private:
    // adds the queued scripts to scripts_ and, when playing, starts every script not started yet (including the ones
    // registered by on_start)
    void flush_register_queue(bool playing);

protected:
    IApplication* app_ = nullptr;
    engine_scene_t scene_{};

    ScriptsMap scripts_{};
    ScriptsQueue scripts_register_queue_{};
    // game objects of the scripts added to scripts_ which haven't started yet, in registration order
    std::vector<engine_game_object_t> scripts_to_start_{};
    ScriptsQueue scripts_unregister_queue_{};
    UserEventSystem user_event_system_;
    bool is_activate_ = true;
};
}