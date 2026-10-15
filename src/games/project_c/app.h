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
};

class AppProjectC : public engine::IApplication
{
public:
    AppProjectC(const PrefabsData& prefabs_data, const AppConfig& config = {});
    ~AppProjectC();

    PrefabResult instantiate_prefab(PrefabType type, engine::IScene* scene);
    bool is_prefab_available(PrefabType type) const;
    // One frame of the game (dt in milliseconds): updates the scenes and applies the game's per-frame rules.
    // The caller owns the loop and calls it between engineFrameBegin() and engineFrameEnd() (main.cpp, tests).
    void update(float dt);

    // what gameplay scripts read as this frame's player input; whoever drives the frames sets it before update()
    const PlayerInput& player_input() const { return player_input_; }
    void set_player_input(const PlayerInput& input) { player_input_ = input; }

private:
    std::array<Prefab, PREFAB_TYPE_COUNT> prefabs_;
    PlayerInput player_input_{};
    bool restart_was_down_ = false;
};
} // namespace project_c