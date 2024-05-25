#pragma once

#include "iapplication.h"
#include "scene_manager.h"
#include "iscene.h"

#include "prefab.h"
#include "animation_controller.h"
#include "prefab_types.h"

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <array>


namespace project_c
{

class AppProjectC : public engine::IApplication
{
public:
    AppProjectC();

    PrefabResult instantiate_prefab(PrefabType type, engine::IScene* scene);
    void run();

private:
    std::array<Prefab, PREFAB_TYPE_COUNT> prefabs_;
};
} // namespace project_c