#pragma once
#include "engine.h"
#include "iscript.h"
#include "utils.h"

#include "event_system.h"

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

class ENGINE_APP_TOOLKIT_API IScene
{
public:
    using ScriptsMap = std::unordered_map<engine_game_object_t, std::unique_ptr<IScript>>;
    using ScriptsQueue = std::deque<IScript*>;

public:
    IScene(engine_application_t app_handle, SceneManager* sc_mng, engine_result_code_t& engine_error_code);
    IScene(const IScene& rhs) = delete;
    IScene(IScene&& rhs) noexcept = default;
    IScene& operator=(const IScene& rhs) = delete;
    IScene& operator=(IScene&& rhs)  noexcept = default;
    virtual ~IScene();

    template<typename T,typename... TArgs>
    T* register_script(TArgs&&... args)
    {
        scripts_register_queue_.push_back(new T(this, args...));
        //std::unique_ptr<IScript> script = std::make_unique<T>(this);
        //const auto game_object = script->get_game_object();
        //scripts_[game_object] = std::move(script);
        return (T*)scripts_register_queue_.back();
        //return (T*)scripts_[game_object].get();
    }

    template<typename T>
    void unregister_script(T* script)
    {
        assert(script);
        scripts_unregister_queue_.push_front(script);
        //const auto game_object = script->get_game_object();
        //scripts_.erase(game_object);
    }

    template<typename T>
    T* get_script(engine_game_object_t go)
    {
        return dynamic_cast<T*>(scripts_.at(go).get());
    }

    template<typename T>
    const T* get_script(engine_game_object_t go) const
    {
        return dynamic_cast<const T*>(scripts_.at(go).get());
    }

    engine_scene_t& get_handle() { return scene_; }
    engine_application_t& get_app_handle() { return app_; }
    SceneManager* get_scene_manager() { return scene_manager_; }


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

protected:
    engine_application_t app_{};
    engine_scene_t scene_{};
    SceneManager* scene_manager_ = nullptr;

    ScriptsMap scripts_{};
    ScriptsQueue scripts_register_queue_{};
    ScriptsQueue scripts_unregister_queue_{};
    InputEventSystem input_event_system_;
    UserEventSystem user_event_system_;
    bool is_activate_ = true;
};
}