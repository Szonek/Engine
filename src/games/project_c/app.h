#pragma once

#include "iapplication.h"
#include "scene_manager.h"
#include "iscene.h"

#include "prefab_types.h"
#include "player_input.h"
#include "player_state.h"

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
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
    // without the editor the game always plays; tests set this to stop and start play sessions (enginePlayModeExit)
    bool play_mode_control = false;
    bool vsync = true;
    // window size in pixels; 0 = the game's default
    std::uint32_t window_width = 0;
    std::uint32_t window_height = 0;
    // the level file LevelScene loads, relative to assets/scenes or absolute
    std::string level_path = "test_level.json";
};

// a level exit asking for the next level: the level file (relative to assets/scenes or absolute) and the player start
// where the player enters it (the player carries their PlayerState, which the app holds)
struct LevelTravel
{
    std::string level_path;
    std::string entrance;
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
    // the data assets the level builder references (loaded at startup, held for the whole run; ENGINE_INVALID_OBJECT_HANDLE
    // when one failed loading): the skeleton warriors' enemy_type and the player's player_tuning
    engine_data_asset_t get_skeleton_warrior_type() const { return skeleton_warrior_type_; }
    engine_data_asset_t get_player_tuning() const { return player_tuning_; }
    // One frame of the game (dt in milliseconds): updates the scenes, then applies scene changes they requested.
    // The caller owns the loop and calls it between engineFrameBegin() and engineFrameEnd() (main.cpp, tests).
    void update(float dt);

    // the level file LevelScene loads (AppConfig::level_path), relative to assets/scenes or absolute, and the id of the
    // player start where the player enters it ("default": where the level starts)
    const std::string& get_level_path() const { return level_path_; }
    const std::string& get_level_entrance() const { return level_entrance_; }
    void set_level_path(std::string path, std::string entrance = "default")
    {
        level_path_ = std::move(path);
        level_entrance_ = std::move(entrance);
    }
    // The player's stats, the only copy: the Player script reads and writes them here, so they outlive its level.
    PlayerState& player_state() { return player_state_; }
    const PlayerState& player_state() const { return player_state_; }
    // What the player entered the current level with, a snapshot taken when they travel (LevelScene): the Player
    // starts the level with it, so a restart gives it back. None when the game starts (a fresh player).
    const std::optional<PlayerState>& get_level_entry_state() const { return level_entry_state_; }
    void set_level_entry_state(std::optional<PlayerState> state) { level_entry_state_ = state; }

    // Level exits ask for the next level here; the level applies the request after its update (LevelScene), so
    // gameplay scripts never depend on the scene class. The first request of a frame wins; false when another one came
    // first. A request no level takes in its frame (made in another kind of scene) is dropped at the end of the frame.
    bool request_level_travel(LevelTravel travel);
    // the pending request, cleared
    std::optional<LevelTravel> take_level_travel_request();

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

protected:
    // The level, entrance and player state a play session may change (a level exit, a restart, the player's hp) are
    // remembered at Play and put back at Stop, so the editor returns to the level it had open with the player it had.
    void on_play_begin() override;
    void on_play_end() override;

private:
    // what on_play_begin remembered
    struct PlaySessionStart
    {
        std::string level_path;
        std::string level_entrance;
        PlayerState player_state;
        std::optional<PlayerState> level_entry_state;
    };

    // the game's component types and the script types scene files and the editor can create (ScriptRegistry)
    void register_script_types();
    // logs why when the type can't be instantiated
    bool is_valid_prefab_type(PrefabType type) const;

    // the engine prefab assets of the models loaded at startup, ENGINE_INVALID_OBJECT_HANDLE for a model which failed loading
    std::array<engine_prefab_t, PREFAB_TYPE_COUNT> prefabs_;
    std::array<std::string, PREFAB_TYPE_COUNT> prefab_paths_;
    engine_data_asset_t skeleton_warrior_type_ = ENGINE_INVALID_OBJECT_HANDLE;
    engine_data_asset_t player_tuning_ = ENGINE_INVALID_OBJECT_HANDLE;
    std::string level_path_;
    std::string level_entrance_ = "default";
    PlayerState player_state_;
    std::optional<PlayerState> level_entry_state_;
    std::optional<LevelTravel> level_travel_request_;
    PlayerInput player_input_{};
    std::function<void()> pending_scene_change_;
    std::optional<PlaySessionStart> play_session_start_;
};
} // namespace project_c