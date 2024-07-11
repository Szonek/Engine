#pragma once

#include "engine.h"
#include "graphics.h"

#include "glm/glm.hpp"

#include <span>

namespace engine
{
struct CameraGpuData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 position;
};

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

class MaterialStaticGeometryUser
{
public: 
    using DrawContext = MaterialStaticGeometryLit::DrawContext;
public:
    MaterialStaticGeometryUser(Shader& shader, std::span<const std::byte> uniform_data, const std::array<const Texture2D*, ENGINE_MATERIAL_CUSTOM_MAX_TEXTURE_BINDING_COUNT>& textures);

    void draw(const Geometry& geometry, const DrawContext& ctx);

private:
    Shader& shader_;
    UniformBuffer ubo_user_;
    std::array<const Texture2D*, ENGINE_MATERIAL_CUSTOM_MAX_TEXTURE_BINDING_COUNT> textures_;
};

class MaterialSkinnedGeometryLit
{
public:
    struct DrawContext
    {
        const UniformBuffer& camera;
        const UniformBuffer& scene;
        const float* model_matrix;
        std::vector<glm::mat4> bone_transforms;

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

class MaterialSpriteUser
{
public:
    using DrawContext = MaterialSprite::DrawContext;
public:
    MaterialSpriteUser(Shader& shader, std::span<const std::byte> uniform_data, const std::array<const Texture2D*, ENGINE_MATERIAL_CUSTOM_MAX_TEXTURE_BINDING_COUNT>& textures);

    void draw(const Geometry& geometry, const DrawContext& ctx);

private:
    Geometry empty_vao_plane_;
    Shader& shader_;
    UniformBuffer ubo_user_;
    std::array<const Texture2D*, ENGINE_MATERIAL_CUSTOM_MAX_TEXTURE_BINDING_COUNT> textures_;
};

}  // namespace engine