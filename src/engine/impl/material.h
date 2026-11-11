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

// Terrains are drawn chunk by chunk: begin() binds everything the chunks of one terrain share, draw_chunk() draws one.
// The chunk geometry is a grid of TERRAIN_CHUNK_CELLS cells whose vertices are displaced by the height texture.
struct TerrainDrawContext
{
    const UniformBuffer& camera;
    const float* model_matrix;
    const Texture2D& heights;
    std::uint32_t sample_count[2];
    float cell_size;
    float texture_tile_size[2];
};

class MaterialTerrainLit
{
public:
    struct DrawContext
    {
        TerrainDrawContext terrain;
        std::uint32_t entity_id;
        const UniformBuffer& scene;
        const float* color_diffuse;
        float shininess;
        const Texture2D& texture_diffuse;
        const Texture2D& texture_specular;
    };
public:
    MaterialTerrainLit();

    void begin(const DrawContext& ctx);
    void draw_chunk(const Geometry& chunk_grid, std::uint32_t origin_x, std::uint32_t origin_z);
    // after the last chunk: the texture units sample like their textures say again
    void end();

private:
    Shader shader_;
    // the terrain's textures repeat over it
    Sampler repeat_sampler_;
};

// the same vertex shader as MaterialTerrainLit, see MaterialStaticGeometryDepthOnly
class MaterialTerrainDepthOnly
{
public:
    MaterialTerrainDepthOnly();

    void begin(const TerrainDrawContext& ctx);
    void draw_chunk(const Geometry& chunk_grid, std::uint32_t origin_x, std::uint32_t origin_z);

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