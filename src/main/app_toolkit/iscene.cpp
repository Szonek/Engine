#include "iscene.h"

#include <fmt/format.h>

namespace
{
engine_result_code_t update_physics(engine_application_t app, engine_scene_t scene, float dt)
{
    auto engine_error_code = engineApplicationFrameSceneUpdatePhysics(app, scene, dt);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Scene physcis update failed. Exiting.\n"));
    }
    return engine_error_code;
}

engine_result_code_t update_graphics(engine_application_t app, engine_scene_t scene, float dt)
{
    auto engine_error_code = engineApplicationFrameSceneUpdateGraphics(app, scene, dt);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Scene update failed. Exiting.\n"));
    }
    return engine_error_code;
}

engine_result_code_t propagate_collisions_events(engine_application_t app, engine_scene_t scene, std::unordered_map<engine_game_object_t, std::unique_ptr<IScript>>& scripts)
{
    std::size_t num_collisions = 0;
    const engine_collision_info_t* collisions_list = nullptr;
    engineSceneGetCollisions(scene, &num_collisions, &collisions_list);
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
        scripts[col.object_a]->on_collision(collision);

        collision.other = col.object_a;
        scripts[col.object_b]->on_collision(collision);
    }
    return ENGINE_RESULT_CODE_OK;
}

engine_result_code_t update_scripts(std::unordered_map<engine_game_object_t, std::unique_ptr<IScript>>& scripts, float dt)
{
    for (auto& [go, script] : scripts)
    {
        script->update(dt);
    }
    return ENGINE_RESULT_CODE_OK;
}

}  // namespace

engine::IScene::IScene(engine_application_t app_handle, engine_result_code_t& engine_error_code)
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

engine::IScene::~IScene()
{
    if (scene_)
    {
        engineSceneDestroy(scene_);
    }
}

void engine::IScene::activate()
{
    is_activate_ = true;
}

void engine::IScene::deactivate()
{
    is_activate_ = false;
}

bool engine::IScene::is_active() const
{
    return is_activate_;
}

engine_result_code_t engine::IScene::update(float dt)
{
    if (!is_active())
    {
        return ENGINE_RESULT_CODE_OK;
    }
    update_physics(app_, scene_, dt);
    propagate_collisions_events(app_, scene_, scripts_);

    update_scripts(scripts_, dt);

    update_graphics(app_, scene_, dt);

    return ENGINE_RESULT_CODE_OK;
}

