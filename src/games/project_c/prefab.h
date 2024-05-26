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

struct ModelInfo
{
    ModelInfo() = default;
    // delete copy constructor and default move constructor
    ModelInfo(const ModelInfo&) = delete;
    ModelInfo& operator=(const ModelInfo&) = delete;
    ModelInfo(ModelInfo&& rhs) noexcept
    {
        std::swap(app, rhs.app);
        std::swap(model_info, rhs.model_info);
        std::swap(geometries, rhs.geometries);
        std::swap(textures, rhs.textures);
        std::swap(materials, rhs.materials);
    }
    ModelInfo& operator=(ModelInfo&& rhs) noexcept
    {
        if (this != &rhs)
        {
            std::swap(app, rhs.app);
            std::swap(model_info, rhs.model_info);
            std::swap(geometries, rhs.geometries);
            std::swap(textures, rhs.textures);
            std::swap(materials, rhs.materials);
        }
        return *this;
    }

    ~ModelInfo()
    {
        if (is_valid())
        {
            engineApplicationReleaseModelDesc(app, &model_info);
        }
    }

    ModelInfo(engine_result_code_t& engine_error_code, engine_application_t& app, std::string_view model_file_name, std::string_view base_dir = "");

    PrefabResult instantiate(engine::IScene* scene) const;

    bool is_valid() const
    {
        return model_info.nodes_count > 0;
    }

private:
    engine_application_t app = nullptr;
    engine_model_desc_t model_info = {};
    std::vector<engine_geometry_t> geometries = {};
    std::vector<engine_texture2d_t> textures = {};
    std::vector<engine_material_t> materials = {};
};
}