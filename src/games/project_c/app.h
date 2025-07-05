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

class AppProjectC : public engine::IApplication
{
public:
    AppProjectC(const std::unordered_map<PrefabType, std::pair<std::string, std::string>>& prefabs_data);
    ~AppProjectC();

    PrefabResult instantiate_prefab(PrefabType type, engine::IScene* scene);
    bool is_prefab_available(PrefabType type) const;
    void run();

private:
    std::array<Prefab, PREFAB_TYPE_COUNT> prefabs_;
};
} // namespace project_c