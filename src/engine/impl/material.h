#pragma once

#include "engine.h"
#include "graphics.h"

#include "glm/glm.hpp"

#include <span>

#include <cstdint>

namespace engine
{
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
        // dithered fade, see dither_fade.h
        float fade = 0.0f;
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
        // index of the first bone matrix of this mesh in the buffer bound to ShaderBindingSlot::SKINNING
        std::uint32_t skinning_matrices_offset;

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

// Depth only materials write no color. They are used to render the shadow map and the depth pre-pass.
// Vertex shaders are the same as in the lit materials, so the depth is exactly the same as the one of the lit draw.
class MaterialStaticGeometryDepthOnly
{
public:
    struct DrawContext
    {
        const UniformBuffer& camera;
        const float* model_matrix;
        // dithered fade, has to match the lit draw (depth pre-pass), 0 for the shadow map
        float fade = 0.0f;
    };
public:
    MaterialStaticGeometryDepthOnly();

    void draw(const Geometry& geometry, const DrawContext& ctx);

private:
    Shader shader_;
};

class MaterialSkinnedGeometryDepthOnly
{
public:
    struct DrawContext
    {
        const UniformBuffer& camera;
        const float* model_matrix;
        // index of the first bone matrix of this mesh in the buffer bound to ShaderBindingSlot::SKINNING
        std::uint32_t skinning_matrices_offset;
    };
public:
    MaterialSkinnedGeometryDepthOnly();

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