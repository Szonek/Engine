#pragma once

#include "iapplication.h"
#include "scene_manager.h"
#include "iscene.h"
#include "animation_controller.h"

#include "model_info.h"

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <array>


namespace project_c
{
enum PrefabType
{
    PREFAB_TYPE_SOLIDER,
    PREFAB_TYPE_SWORD,
    PREFAB_TYPE_ORC,
    PREFAB_TYPE_BARREL,
    PREFAB_TYPE_FLOOR,
    PREFAB_TYPE_FLOOR_DETAIL,
    PREFAB_TYPE_WALL,
    PREFAB_TYPE_CUBE,
    PREFAB_TYPE_COUNT
};

class AppProjectC : public engine::IApplication
{
public:
    AppProjectC();

    engine_game_object_t instantiate_prefab(PrefabType type, engine::IScene* scene);
    void run();

private:
    std::array<ModelInfo, PREFAB_TYPE_COUNT> prefabs_;
};
} // namespace project_c