#pragma once

#include "iapplication.h"
#include "scene_manager.h"
#include "iscene.h"

#include "prefab_types.h"
#include "player_input.h"

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <array>
#include <functional>
#include <unordered_map>

namespace project_c
{
// prefab type -> path of the model file under assets/models (the prefab's asset ID)
using PrefabsData = std::unordered_map<PrefabType, std::string>;
// the models the game loads at startup
PrefabsData default_prefabs_data();

struct AppConfig
{
    bool enable_editor = true;
    bool hidden_window = false;
    bool vsync = true;
    // window size in pixels; 0 = the game's default
    std::uint32_t window_width = 0;
    std::uint32_t window_height = 0;
};

class AppProjectC : public engine::IApplication
{
public:
    AppProjectC(const PrefabsData& prefabs_data, const AppConfig& config = {});
    ~AppProjectC();

    // an instance which is part of the scene's content: the level places these
    PrefabResult instantiate_prefab(PrefabType type);
    // a transient instance (never saved, no persistent IDs): scripts spawn these at runtime
    PrefabResult spawn_prefab(PrefabType type);
    bool is_prefab_available(PrefabType type) const;
    // the asset ID of an asset of the type's model, e.g. one of its meshes: "<model path>#<name>"
    std::string model_asset_id(PrefabType type, std::string_view name) const;
    // One frame of the game (dt in milliseconds): updates the scenes, then applies scene changes they requested.
    // The caller owns the loop and calls it between engineFrameBegin() and engineFrameEnd() (main.cpp, tests).
    void update(float dt);

    // what gameplay scripts read as this frame's player input; whoever drives the frames sets it before update()
    const PlayerInput& player_input() const { return player_input_; }
    void set_player_input(const PlayerInput& input) { player_input_ = input; }

    // Replaces scene T with a fresh instance at the end of this frame's update(): a scene can't destroy and
    // re-create itself while it is being updated (e.g. a level restarting itself).
    template<typename T>
    void request_scene_reload()
    {
        pending_scene_change_ = [this]()
        {
            unregister_scene(T::get_name());
            register_scene<T>()->activate();
        };
    }

private:
    // logs why when the type can't be instantiated
    bool is_valid_prefab_type(PrefabType type) const;

    // the engine prefab assets of the models loaded at startup, ENGINE_INVALID_OBJECT_HANDLE for a model which failed loading
    std::array<engine_prefab_t, PREFAB_TYPE_COUNT> prefabs_;
    std::array<std::string, PREFAB_TYPE_COUNT> prefab_paths_;
    PlayerInput player_input_{};
    std::function<void()> pending_scene_change_;
};
} // namespace project_c