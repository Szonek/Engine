#pragma once

#include "iapplication.h"
#include "scene_manager.h"
#include "iscene.h"

#include "prefab.h"
#include "prefab_types.h"
#include "player_input.h"

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <array>
#include <unordered_map>

namespace project_c
{
// prefab type -> { model file, base dir under assets/models }
using PrefabsData = std::unordered_map<PrefabType, std::pair<std::string, std::string>>;
// the models the game loads at startup
PrefabsData default_prefabs_data();

struct AppConfig
{
    bool enable_editor = true;
    bool hidden_window = false;
    // window size in pixels; 0 = the game's default
    std::uint32_t window_width = 0;
    std::uint32_t window_height = 0;
    // register and activate TestScene (the game's level); tests register their own scenes instead
    bool register_default_scene = true;
    // false: the keyboard and mouse are never read, player input only comes from set_player_input() (tests)
    bool read_input_devices = true;
};

class AppProjectC : public engine::IApplication
{
public:
    AppProjectC(const PrefabsData& prefabs_data, const AppConfig& config = {});
    ~AppProjectC();

    PrefabResult instantiate_prefab(PrefabType type, engine::IScene* scene);
    bool is_prefab_available(PrefabType type) const;
    void run();
    // one frame of the game: refresh player input, then update the scenes (dt in milliseconds)
    void update(float dt);

    const PlayerInput& player_input() const { return player_input_; }
    // input seen by gameplay from now on; the next update() overwrites it unless read_input_devices is off
    void set_player_input(const PlayerInput& input) { player_input_ = input; }

private:
    std::array<Prefab, PREFAB_TYPE_COUNT> prefabs_;
    bool read_input_devices_ = true;
    PlayerInput player_input_{};
};
} // namespace project_c