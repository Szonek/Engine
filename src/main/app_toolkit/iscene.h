#pragma once
#include "engine.h"
#include "iscript.h"
#include "utils.h"

#include "event_system.h"

#include <unordered_map>

namespace engine
{
class SceneManager;

class IScene
{
public:
    using ScriptsMap = std::unordered_map<engine_game_object_t, std::unique_ptr<IScript>>;
public:
    IScene(engine_application_t app_handle, SceneManager* sc_mng, engine_result_code_t& engine_error_code);
    IScene(const IScene& rhs) = delete;
    IScene(IScene&& rhs) noexcept = default;
    IScene& operator=(const IScene& rhs) = delete;
    IScene& operator=(IScene&& rhs)  noexcept = default;
    ~IScene();

    template<typename T>
    T* register_script()
    {
        std::unique_ptr<IScript> script = std::make_unique<T>(this);
        const auto game_object = script->get_game_object();
        scripts_[game_object] = std::move(script);

        return (T*)scripts_[game_object].get();
    }

    engine_scene_t& get_handle() { return scene_; }
    engine_application_t& get_app_handle() { return app_; }
    SceneManager* get_scene_manager() { return scene_manager_; }

    virtual void activate();
    virtual void deactivate();
    virtual bool is_active() const;
    virtual engine_result_code_t update(float dt);

protected:
    engine_application_t app_{};
    engine_scene_t scene_{};
    SceneManager* scene_manager_ = nullptr;

    ScriptsMap scripts_{};
    InputEventSystem input_event_system_;
    bool is_activate_ = true;
};
}