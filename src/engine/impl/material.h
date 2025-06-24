#pragma once

#include "engine.h"
#include "graphics.h"

#include "glm/glm.hpp"

#include <span>

#include <cstdint>

namespace engine
{
struct SceneGpuData
{
    std::uint32_t direction_light_count = 0;
    std::uint32_t point_light_count = 0;
    std::uint32_t spot_light_count = 0;
    float pad3_;
};

class MaterialStaticGeometryLit
{
public:
    struct DrawContext
    {
        std::uint32_t entity_id;
        const UniformBuffer& camera;
        const UniformBuffer& scene;
        const float* model_matrix;
        
        const float* color_diffuse;
        float shininess;
        const Texture2D& texture_diffuse;
        const Texture2D& texture_specular;
    };
public:
    MaterialStaticGeometryLit();

    void draw(const Geometry& geometry, const DrawContext& ctx);

private:
    Shader shader_;
};


class MaterialStaticGeometryUnlit
{
public:
    struct DrawContext
    {
        const UniformBuffer& camera;
        const float* model_matrix;

        const float* color_diffuse;
        const Texture2D& texture_diffuse;
    };
public:
    MaterialStaticGeometryUnlit();

    void draw(const Geometry& geometry, const DrawContext& ctx);

private:
    Shader shader_;
};

class MaterialSkinnedGeometryLit
{
public:
    struct DrawContext
    {
        std::uint32_t entity_id;
        const UniformBuffer& camera;
        const UniformBuffer& scene;
        const float* model_matrix;
        std::span<const glm::mat4> bone_transforms;

        const float* color_diffuse;
        float shininess;
        const Texture2D& texture_diffuse;
        const Texture2D& texture_specular;
    };
public:
    MaterialSkinnedGeometryLit();

    void draw(const Geometry& geometry, const DrawContext& ctx);

private:
    Shader shader_;
};

class MaterialSkinnedGeometryUnlit
{
public:
    struct DrawContext
    {
        const UniformBuffer& camera;
        const float* model_matrix;
        std::vector<glm::mat4> bone_transforms;

        const float* color_diffuse;
        const Texture2D& texture_diffuse;
    };
public:
    MaterialSkinnedGeometryUnlit();

    void draw(const Geometry& geometry, const DrawContext& ctx);

private:
    Shader shader_;
};

class MaterialSprite
{
public:
    struct DrawContext
    {
        const UniformBuffer& camera;
        const UniformBuffer& scene;

        glm::vec3 world_position;
        glm::vec3 scale;
    };
public:
    MaterialSprite();
    void draw(const DrawContext& ctx);

private:
    Shader shader_;
    Geometry empty_vao_plane_;
};

}  // namespace engine