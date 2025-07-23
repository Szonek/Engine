#include "app.h"
#include "iscene.h"

#include "scenes/scene_test.h"

#include "scripts/enemy_script.h"

#include <chrono>
#include <map>
#include <fmt/format.h>

namespace
{
inline engine_application_create_desc_t app_cd()
{
    engine_application_create_desc_t app_cd{};
    app_cd.name = "Project_C";
    app_cd.asset_store_path = "C:\\WORK\\OpenGLPlayground\\assets";
    app_cd.width = K_IS_ANDROID ? 0 : 2280 / 2;
    app_cd.height = K_IS_ANDROID ? 0 : 1080 / 2;
    app_cd.fullscreen = K_IS_ANDROID;
    app_cd.enable_editor = true;
    return app_cd;
}
}  // namespace anonymous

project_c::AppProjectC::AppProjectC(const std::unordered_map<PrefabType, std::pair<std::string, std::string>>& prefabs_data)
    : engine::IApplication(app_cd())
{
    const auto load_start = std::chrono::high_resolution_clock::now();

    if (engineCreateFontFromFile("tahoma.ttf", "tahoma_font") != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt load font!\n"));
        return;
    }

    for (const auto& [type, file_and_basedir] : prefabs_data)
    {
        const auto& [model_file_name, base_dir] = file_and_basedir;
        engine_result_code_t engine_error_code = ENGINE_RESULT_CODE_FAIL;
        prefabs_[type] = std::move(Prefab(engine_error_code, model_file_name, base_dir));
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            log(fmt::format("Failed loading prefab: {}\n", type));
        }
    }

    const auto load_end = std::chrono::high_resolution_clock::now();
    const auto ms_load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
    log(fmt::format("Model loading took: {}\n", ms_load_time));

    register_scene<project_c::TestScene>();
}

project_c::AppProjectC::~AppProjectC()
{
}

project_c::PrefabResult project_c::AppProjectC::instantiate_prefab(PrefabType type, engine::IScene* scene)
{
    if (type >= PREFAB_TYPE_COUNT)
    {
        log(fmt::format("Invalid prefab type: {}\n", type));
        return { ENGINE_INVALID_GAME_OBJECT_ID };
    }

    auto& prefab = prefabs_[type];
    if (!prefab.is_valid())
    {
        log(fmt::format("Prefab: {} is not valid\n", type));
        return { ENGINE_INVALID_GAME_OBJECT_ID };
    }
    return prefab.instantiate(scene);
}

bool project_c::AppProjectC::is_prefab_available(PrefabType type) const
{
    return prefabs_[type].is_valid();
}

void project_c::AppProjectC::run()
{
    struct fps_counter_t
    {
        float frames_total_time = 0.0f;
        std::uint32_t frames_count = 0;
    };
    fps_counter_t fps_counter{};

    while (true)
    {
        const auto frame_begin = engineFrameBegine();

        if (frame_begin.events & ENGINE_EVENT_QUIT)
        {
            log(fmt::format("Engine requested app quit. Exiting.\n"));
            break;
        }

        if (engineIsKeyboardButtonDown(ENGINE_KEYBOARD_KEY_ESCAPE))
        {
            log(fmt::format("User pressed ESCAPE key. Exiting.\n"));
            break;
        }

        fps_counter.frames_count += 1;
        fps_counter.frames_total_time += frame_begin.delta_time;
        if (fps_counter.frames_total_time > 1000.0f)
        {
            log(fmt::format("FPS: {}, latency: {} ms. \n",
                fps_counter.frames_count, fps_counter.frames_total_time / fps_counter.frames_count));
            fps_counter = {};
        }

        auto scene = get_scene(TestScene::get_name());
        update_scenes(frame_begin.delta_time);

        const auto frame_end = engineFrameEnd();
        if (!frame_end.success)
        {
            log(fmt::format("Frame not finished sucesfully. Exiting.\n"));
            break;
        }
    }
}