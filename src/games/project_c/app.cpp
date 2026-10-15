#include "app.h"
#include "iscene.h"

#include "scenes/scene_test.h"

#include "scripts/enemy_script.h"

#include <chrono>
#include <map>
#include <fmt/format.h>

namespace
{
inline engine_application_create_desc_t app_cd(const project_c::AppConfig& config)
{
    engine_application_create_desc_t app_cd{};
    app_cd.name = "Project_C";
    // assets of the source tree this binary was built from (set by CMake), so worktrees/branches use matching shaders
    app_cd.asset_store_path = PROJECT_C_ASSETS_DIR;
    app_cd.width = K_IS_ANDROID ? 0 : (config.window_width ? config.window_width : 2280 / 2);
    app_cd.height = K_IS_ANDROID ? 0 : (config.window_height ? config.window_height : 1080 / 2);
    app_cd.fullscreen = K_IS_ANDROID;
    app_cd.enable_editor = config.enable_editor;
    app_cd.hidden_window = config.hidden_window;
    return app_cd;
}
}  // namespace anonymous

project_c::PrefabsData project_c::default_prefabs_data()
{
    return
    {
        // player
        { project_c::PREFAB_TYPE_ROGUE,         { "Rogue.glb", "KayKit_Adventurers_1.0_FREE/Characters/gltf" }},

        // enemies
        { project_c::PREFAB_TYPE_SKELETON_WARRIOR, { "Skeleton_Warrior.glb", "KayKit_Skeletons_1.0_FREE/characters/gltf" }},
        { project_c::PREFAB_TYPE_SKELETON_BLADE, { "Skeleton_Blade.gltf", "KayKit_Skeletons_1.0_FREE/assets/gltf" }},

        // scene assets
        { project_c::PREFAB_TYPE_FLOOR,          { "Floor_Prototype.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
        //{ project_c::PREFAB_FLOOR_OUTSIDE_REGION, { "Floor.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},

        //{ project_c::PREFAB_TYPE_WALL,      { "Wall.gltf",   "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
        { project_c::PREFAB_TYPE_BOX,       { "Box_B.gltf",  "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
        { project_c::PREFAB_TYPE_COIN_GOLD, { "Coin_A.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},

        // building pieces
        { project_c::PREFAB_TYPE_BUILDING_WALL,   { "Primitive_Wall.gltf",   "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
        { project_c::PREFAB_TYPE_BUILDING_WINDOW, { "Primitive_Window.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
        { project_c::PREFAB_TYPE_BUILDING_ROOF,   { "Primitive_Floor.gltf",  "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
        { project_c::PREFAB_TYPE_TRAVERSAL_MARKER, { "Primitive_Cube_Small.gltf", "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf" }},
    };
}

project_c::AppProjectC::AppProjectC(const PrefabsData& prefabs_data, const AppConfig& config)
    : engine::IApplication(app_cd(config))
{
    const auto load_start = std::chrono::high_resolution_clock::now();

    if (engineFontCreateFromFile("tahoma.ttf", "tahoma_font") != ENGINE_RESULT_CODE_OK)
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

    if (config.register_default_scene)
    {
        auto scene = register_scene<project_c::TestScene>();
        scene->activate();
    }
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
    bool restart_was_down = false;

    while (true)
    {
        const auto frame_begin = engineFrameBegin();

        if (frame_begin.events & ENGINE_EVENT_QUIT)
        {
            log(fmt::format("Engine requested app quit. Exiting.\n"));
            break;
        }

        if (engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_ESCAPE))
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

        auto* scene = static_cast<TestScene*>(get_scene(TestScene::get_name()));
        update_scenes(frame_begin.delta_time);
        if (scene)
        {
            scene->set_gameplay_ui_visible(engineEditorIsGameView());
        }
        const bool restart_is_down = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_R);
        if (scene && scene->is_encounter_over() && restart_is_down && !restart_was_down)
        {
            unregister_scene(TestScene::get_name());
            auto* restarted_scene = register_scene<TestScene>();
            restarted_scene->activate();
        }
        restart_was_down = restart_is_down;

        const auto frame_end = engineFrameEnd();
        if (!frame_end.success)
        {
            log(fmt::format("Frame not finished sucesfully. Exiting.\n"));
            break;
        }
    }
}