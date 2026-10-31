#include "app.h"

#include "scripts/building_script.h"
#include "scripts/camera_script.h"
#include "scripts/encounter_script.h"
#include "scripts/enemy_script.h"
#include "scripts/enemy_spawner_script.h"
#include "scripts/enviorment_script.h"
#include "scripts/game_components.h"
#include "scripts/gameplay_hud_script.h"
#include "scripts/interactable_script.h"
#include "scripts/level_exit_script.h"
#include "scripts/player_script.h"

#include <chrono>
#include <map>
#include <fmt/format.h>

namespace
{
// data assets, relative to assets/data
constexpr const char* SKELETON_WARRIOR_TYPE_PATH = "enemies/skeleton_warrior.json";
constexpr const char* PLAYER_TUNING_PATH = "player/tuning.json";

inline engine_application_create_desc_t app_cd(const project_c::AppConfig& config)
{
    engine_application_create_desc_t app_cd{};
    app_cd.name = "Project_C";
    // assets of the source tree this binary was built from (set by CMake), so worktrees/branches use matching shaders
    app_cd.asset_store_path = PROJECT_C_ASSETS_DIR;
    app_cd.width = config.window_width ? config.window_width : 2280 / 2;
    app_cd.height = config.window_height ? config.window_height : 1080 / 2;
    app_cd.enable_editor = config.enable_editor;
    app_cd.hidden_window = config.hidden_window;
    app_cd.play_mode_control = config.play_mode_control;
    app_cd.disable_vsync = !config.vsync;
    return app_cd;
}
}  // namespace anonymous

project_c::PrefabsData project_c::default_prefabs_data()
{
    return
    {
        // player
        { project_c::PREFAB_TYPE_ROGUE,         "KayKit_Adventurers_1.0_FREE/Characters/gltf/Rogue.glb" },

        // enemies (their models are named by their enemy types, assets/data/enemies)
        { project_c::PREFAB_TYPE_SKELETON_BLADE, "KayKit_Skeletons_1.0_FREE/assets/gltf/Skeleton_Blade.gltf" },

        // scene assets
        { project_c::PREFAB_TYPE_FLOOR,          "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Floor_Prototype.gltf" },
        //{ project_c::PREFAB_FLOOR_OUTSIDE_REGION, "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Floor.gltf" },

        //{ project_c::PREFAB_TYPE_WALL,      "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Wall.gltf" },
        { project_c::PREFAB_TYPE_BOX,       "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Box_B.gltf" },
        { project_c::PREFAB_TYPE_COIN_GOLD, "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Coin_A.gltf" },

        // building pieces
        { project_c::PREFAB_TYPE_BUILDING_WALL,   "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Primitive_Wall.gltf" },
        { project_c::PREFAB_TYPE_BUILDING_WINDOW, "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Primitive_Window.gltf" },
        { project_c::PREFAB_TYPE_BUILDING_ROOF,   "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Primitive_Floor.gltf" },
        { project_c::PREFAB_TYPE_TRAVERSAL_MARKER, "KayKit_Prototype_Bits_1.0_FREE/Assets/gltf/Primitive_Cube_Small.gltf" },
    };
}

project_c::AppProjectC::AppProjectC(const PrefabsData& prefabs_data, const AppConfig& config)
    : engine::IApplication(app_cd(config))
    , level_path_(config.level_path)
{
    register_script_types();
    // logged when they fail: enemies and the player then have the types' default values
    skeleton_warrior_type_ = engineDataAssetLoad(SKELETON_WARRIOR_TYPE_PATH);
    player_tuning_ = engineDataAssetLoad(PLAYER_TUNING_PATH);
    prefabs_.fill(ENGINE_INVALID_OBJECT_HANDLE);
    const auto load_start = std::chrono::high_resolution_clock::now();

    if (engineFontCreateFromFile("tahoma.ttf", "tahoma_font") != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt load font!\n"));
        return;
    }

    for (const auto& [type, path] : prefabs_data)
    {
        prefab_paths_[type] = path;
        if (enginePrefabLoad(ENGINE_MODEL_SPECIFICATION_GLTF_2, path.c_str(), &prefabs_[type]) != ENGINE_RESULT_CODE_OK)
        {
            log(fmt::format("Failed loading prefab: {}\n", path));
        }
    }

    const auto load_end = std::chrono::high_resolution_clock::now();
    const auto ms_load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
    log(fmt::format("Model loading took: {}\n", ms_load_time));

}

void project_c::AppProjectC::register_script_types()
{
    register_game_components();
    const auto& components = game_components();
    // the names scene files save
    auto& registry = scripts();
    registry.add<CameraScript>("camera");
    registry.add<Encounter>("encounter");
    registry.add<GameplayHud>("gameplay_hud");
    registry.add<Floor>("floor");
    registry.add<Player>("player", { components.player_config });
    registry.add<Enemy>("enemy", { components.enemy_config });
    registry.add<Chest>("chest");
    registry.add<Weapon>("weapon");
    registry.add<House>("house", { components.house_config });
    registry.add<BuildingPart>("building_part", { components.building_part_config });
    registry.add<LevelExit>("level_exit", { components.level_exit });
    registry.add<EnemySpawner>("enemy_spawner", { components.enemy_spawner });
    if (!registry.register_component())
    {
        log("Failed registering the script component: scene files can't create scripts.\n");
    }
}

project_c::AppProjectC::~AppProjectC()
{
    for (const auto asset : { skeleton_warrior_type_, player_tuning_ })
    {
        if (asset != ENGINE_INVALID_OBJECT_HANDLE)
        {
            engineDataAssetRelease(asset);
        }
    }
    for (const auto prefab : prefabs_)
    {
        if (prefab != ENGINE_INVALID_OBJECT_HANDLE)
        {
            enginePrefabRelease(prefab);
        }
    }
}

project_c::PrefabResult project_c::AppProjectC::instantiate_prefab(PrefabType type)
{
    return { is_valid_prefab_type(type) ? enginePrefabInstantiate(prefabs_[type]) : ENGINE_INVALID_GAME_OBJECT_ID };
}

project_c::PrefabResult project_c::AppProjectC::spawn_prefab(PrefabType type)
{
    return { is_valid_prefab_type(type) ? enginePrefabInstantiateTransient(prefabs_[type]) : ENGINE_INVALID_GAME_OBJECT_ID };
}

bool project_c::AppProjectC::is_valid_prefab_type(PrefabType type) const
{
    if (type >= PREFAB_TYPE_COUNT)
    {
        log(fmt::format("Invalid prefab type: {}\n", fmt::underlying(type)));
        return false;
    }

    if (!is_prefab_available(type))
    {
        log(fmt::format("Prefab: {} is not valid\n", fmt::underlying(type)));
        return false;
    }
    return true;
}

bool project_c::AppProjectC::is_prefab_available(PrefabType type) const
{
    return prefabs_[type] != ENGINE_INVALID_OBJECT_HANDLE;
}

std::string project_c::AppProjectC::model_asset_id(PrefabType type, std::string_view name) const
{
    return fmt::format("{}#{}", prefab_paths_[type], name);
}

bool project_c::AppProjectC::request_level_travel(LevelTravel travel)
{
    if (level_travel_request_)
    {
        return false;
    }
    level_travel_request_ = std::move(travel);
    return true;
}

std::optional<project_c::LevelTravel> project_c::AppProjectC::take_level_travel_request()
{
    auto request = std::move(level_travel_request_);
    level_travel_request_.reset();
    return request;
}

void project_c::AppProjectC::on_play_begin()
{
    play_session_start_ = PlaySessionStart{ level_path_, level_entrance_, player_state_, level_entry_state_ };
}

void project_c::AppProjectC::on_play_end()
{
    if (!play_session_start_)
    {
        return;
    }
    level_path_ = std::move(play_session_start_->level_path);
    level_entrance_ = std::move(play_session_start_->level_entrance);
    player_state_ = play_session_start_->player_state;
    level_entry_state_ = play_session_start_->level_entry_state;
    play_session_start_.reset();
}

void project_c::AppProjectC::update(float dt)
{
    update_scenes(dt);
    // only a LevelScene takes travel requests; one left over would fire in whichever level loads next
    if (level_travel_request_)
    {
        log(fmt::format("Nothing took the travel to the level {}; it is dropped.\n", level_travel_request_->level_path));
        level_travel_request_.reset();
    }

    if (pending_scene_change_)
    {
        const auto change = std::move(pending_scene_change_);
        pending_scene_change_ = nullptr;
        change();
    }
}
