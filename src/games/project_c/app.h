#pragma once

#include "iapplication.h"
#include "scene_manager.h"
#include "iscene.h"

#include "prefab.h"
#include "prefab_types.h"

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
};

class AppProjectC : public engine::IApplication
{
public:
    AppProjectC(const PrefabsData& prefabs_data, const AppConfig& config = {});
    ~AppProjectC();

    PrefabResult instantiate_prefab(PrefabType type, engine::IScene* scene);
    bool is_prefab_available(PrefabType type) const;
    void run();

private:
    std::array<Prefab, PREFAB_TYPE_COUNT> prefabs_;
};
} // namespace project_c