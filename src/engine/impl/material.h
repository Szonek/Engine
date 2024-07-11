#pragma once

#include "engine.h"
#include "graphics.h"

#include "glm/glm.hpp"

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

}  // namespace engine