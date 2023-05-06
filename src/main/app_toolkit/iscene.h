#pragma once
#include "engine.h"
#include "iscript.h"
#include "utils.h"

#include <unordered_map>
namespace engine
{
class IScene
{
public:
    IScene(engine_application_t app_handle, engine_result_code_t& engine_error_code);
    IScene(const IScene& rhs) = delete;
    IScene(IScene&& rhs) noexcept = default;
    IScene& operator=(const IScene& rhs) = delete;
    IScene& operator=(IScene&& rhs)  noexcept = default;
    ~IScene();

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
    std::unordered_map<engine_game_object_t, std::unique_ptr<IScript>> scripts_;


protected:
    engine_scene_t scene_{};
    bool is_activate_ = true;
};
}