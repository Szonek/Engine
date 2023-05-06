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

    virtual engine_result_code_t update(float dt);

private:
    struct fps_counter_t
    {
        float frames_total_time = 0.0f;
        std::uint32_t frames_count = 0;
    };

private:
    engine_application_t app_{};
    std::unordered_map<engine_game_object_t, std::unique_ptr<IScript>> scripts_;
    fps_counter_t fps_counter_{};

protected:
    engine_scene_t scene_{};
};
}