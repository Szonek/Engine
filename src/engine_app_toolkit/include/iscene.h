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
        return dynamic_cast<T*>(scripts_.at(go).get());
    }

    template<typename T>
    const T* get_script(engine_game_object_t go) const
    {
        return dynamic_cast<const T*>(scripts_.at(go).get());
    }

    IApplication* get_app() { return app_; }
    engine_scene_t& get_handle() { return scene_; }
    engine_application_t get_app_handle();

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
    IApplication* app_ = nullptr;
    engine_scene_t scene_{};

    ScriptsMap scripts_{};
    ScriptsQueue scripts_register_queue_{};
    ScriptsQueue scripts_unregister_queue_{};
    UserEventSystem user_event_system_;
    bool is_activate_ = true;
};
}