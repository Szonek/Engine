#pragma once
#include "engine.h"
#include "iscript.h"
#include "utils.h"

#include "event_types_defs.h"

#include <unordered_map>

namespace engine
{
class InputEventSystem
{
public:
    struct UpdateResult
    {
        bool pointer_moved_event = false;
        bool pointer_clicked_event = false;
        PointerEventData event_data = {};
    };

    InputEventSystem(engine_application_t app_handle)
        : app_(app_handle)
    {

    }

    UpdateResult update();

    engine_coords_2d_t mouse_coords_prev_{};
    std::array<bool, ENGINE_MOUSE_BUTTON_COUNT> mouse_down_state_prev_{};

private:
    engine_application_t app_;
};


class IScene
{
public:
    using ScriptsMap = std::unordered_map<engine_game_object_t, std::unique_ptr<IScript>>;
public:
    IScene(engine_application_t app_handle, engine_result_code_t& engine_error_code);
    IScene(const IScene& rhs) = delete;
    IScene(IScene&& rhs) noexcept = default;
    IScene& operator=(const IScene& rhs) = delete;
    IScene& operator=(IScene&& rhs)  noexcept = default;
    ~IScene();

    engine_scene_t get_handle() { return scene_; }
    ScriptsMap& get_scripts_map() { return scripts_; }

    template<typename T>
    T* register_script()
    {
        std::unique_ptr<IScript> script = std::make_unique<T>(app_, scene_);
        const auto game_object = script->get_game_object();
        scripts_[game_object] = std::move(script);
        return (T*)scripts_[game_object].get();
    }


    virtual void activate();
    virtual void deactivate();
    virtual bool is_active() const;
    virtual engine_result_code_t update(float dt);

private:
    engine_application_t app_{};
    ScriptsMap scripts_{};
    InputEventSystem input_event_system_;

protected:
    engine_scene_t scene_{};
    bool is_activate_ = true;
};
}