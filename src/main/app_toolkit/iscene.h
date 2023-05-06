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
    IScene(engine_application_t app_handle, engine_result_code_t& engine_error_code)
        : app_(app_handle)
    {
        engine_error_code = engineSceneCreate(&scene_);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineSceneDestroy(scene_);
            log(fmt::format("Couldnt create scene!\n"));
            return;
        }
        scripts_.reserve(1024);
    }
    IScene(const IScene& rhs) = delete;
    IScene(IScene&& rhs) noexcept = default;
    IScene& operator=(const IScene& rhs) = delete;
    IScene& operator=(IScene&& rhs)  noexcept = default;

    ~IScene()
    {
        if (scene_)
        {
            engineSceneDestroy(scene_);
        }
    }

    template<typename T>
    T* register_script()
    {
        std::unique_ptr<IScript> script = std::make_unique<T>(app_, scene_);
        const auto game_object = script->get_game_object();
        scripts_[game_object] = std::move(script);
        return (T*)scripts_[game_object].get();
    }

    virtual engine_result_code_t update(float dt)
    {
        //fps_counter_.frames_count += 1;
        //fps_counter_.frames_total_time += dt;
        //if (fps_counter_.frames_total_time > 1000.0f)
        //{
        //    log(fmt::format("FPS: {}, latency: {} ms. \n",
        //        fps_counter.frames_count, fps_counter.frames_total_time / fps_counter.frames_count));
        //    fps_counter = {};

        //    const auto log_score = fmt::format("Left: {}, Right: {}\n", left_player_script.get_score(), right_player_script.get_score());
        //    engineLog(log_score.c_str());
        //}

        update_physics(dt);
        propagate_collisions_events(dt);

        update_scripts(dt);

        update_graphics(dt);

        return ENGINE_RESULT_CODE_OK;
    }

private:
    engine_result_code_t update_physics(float dt)
    {
        auto engine_error_code = engineApplicationFrameSceneUpdatePhysics(app_, scene_, dt);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            log(fmt::format("Scene physcis update failed. Exiting.\n"));
        }
        return engine_error_code;
    }

    engine_result_code_t update_graphics(float dt)
    {
        auto engine_error_code  = engineApplicationFrameSceneUpdateGraphics(app_, scene_, dt);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            log(fmt::format("Scene update failed. Exiting.\n"));
        }
        return engine_error_code;
    }

    engine_result_code_t update_scripts(float dt)
    {
        for (auto& [go, script] : scripts_)
        {
            script->update(dt);
        }
        return ENGINE_RESULT_CODE_OK;
    }

    engine_result_code_t propagate_collisions_events(float dt)
    {
        std::size_t num_collisions = 0;
        const engine_collision_info_t* collisions_list = nullptr;
        engineSceneGetCollisions(scene_, &num_collisions, &collisions_list);
        for (std::size_t i = 0; i < num_collisions; i++)
        {
            const auto& col = collisions_list[i];
            IScript::collision_t collision{};
            collision.contact_points.resize(col.contact_points_count);
            for (std::size_t j = 0; j < col.contact_points_count; j++)
            {
                collision.contact_points[j].lifetime = col.contact_points[j].lifetime;
                collision.contact_points[j].point[0] = col.contact_points[j].point_object_a[0];
                collision.contact_points[j].point[1] = col.contact_points[j].point_object_a[1];
                collision.contact_points[j].point[2] = col.contact_points[j].point_object_a[2];
            }

            collision.other = col.object_b;
            scripts_[col.object_a]->on_collision(collision);

            collision.other = col.object_a;
            scripts_[col.object_b]->on_collision(collision);
        }
        return ENGINE_RESULT_CODE_OK;
    }

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