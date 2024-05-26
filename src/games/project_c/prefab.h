#pragma once
#include "animation_controller.h"

#include <engine.h>

#include <vector>
#include <string_view>

namespace engine
{
class IScene;
}

namespace project_c
{

struct PrefabResult
{
    engine_game_object_t go;
    AnimationController anim_controller;
};

struct Prefab
{
    Prefab(engine_result_code_t& engine_error_code, engine_application_t& app, std::string_view model_file_name, std::string_view base_dir = "");
    Prefab() = default;
    // delete copy constructor and default move constructor
    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;
    Prefab(Prefab&& rhs) noexcept;
    Prefab& operator=(Prefab&& rhs) noexcept;
    ~Prefab();

    PrefabResult instantiate(engine::IScene* scene) const;
    bool is_valid() const;

private:
    engine_application_t app_ = nullptr;
    engine_model_desc_t model_info_ = {};
    std::vector<engine_geometry_t> geometries_ = {};
    std::vector<engine_texture2d_t> textures_ = {};
    std::vector<engine_material_t> materials_ = {};
};
}