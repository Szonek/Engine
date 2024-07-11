#include "material.h"
#include "math_helpers.h"

engine::MaterialStaticGeometryLit::MaterialStaticGeometryLit(Shader& shader)
    : shader_(shader)
    , ubo_camera_(UniformBuffer(sizeof(CameraGpuData)))
    , ubo_scene_(UniformBuffer(sizeof(SceneGpuData)))
{
}

void engine::MaterialStaticGeometryLit::draw(const Geometry& geometry, const DrawContext& ctx)
{
    // ToDo: this buffer uploads can be optimized by doing it once per frame
    {
        BufferMapContext<CameraGpuData, UniformBuffer> camera_ubo(ubo_camera_, false, true);
        *camera_ubo.data = ctx.camera;
    }
    {
        BufferMapContext<SceneGpuData, UniformBuffer> scene_ubo(ubo_scene_, false, true);
        *scene_ubo.data = ctx.scene;
    }
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ubo_camera_, 0);
    shader_.set_uniform_block("SceneData", &ubo_scene_, 1);

    shader_.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });

    shader_.set_uniform_f4("diffuse_color", {ctx.color_diffuse, 4});
    shader_.set_uniform_f1("shininess", ctx.shininess);

    shader_.set_texture("texture_diffuse", &ctx.texture_diffuse);
    shader_.set_texture("texture_specular", &ctx.texture_specular);

    geometry.bind();
    geometry.draw(Geometry::Mode::eTriangles);
}

engine::MaterialSkinnedGeometryLit::MaterialSkinnedGeometryLit(Shader& shader)
    : shader_(shader)
    , ubo_camera_(UniformBuffer(sizeof(CameraGpuData)))
    , ubo_scene_(UniformBuffer(sizeof(SceneGpuData)))
{

}

void engine::MaterialSkinnedGeometryLit::draw(const Geometry& geometry, const DrawContext& ctx)
{
    // ToDo: this buffer uploads can be optimized by doing it once per frame
    {
        BufferMapContext<CameraGpuData, UniformBuffer> camera_ubo(ubo_camera_, false, true);
        *camera_ubo.data = ctx.camera;
    }
    {
        BufferMapContext<SceneGpuData, UniformBuffer> scene_ubo(ubo_scene_, false, true);
        *scene_ubo.data = ctx.scene;
    }
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ubo_camera_, 0);
    shader_.set_uniform_block("SceneData", &ubo_scene_, 1);

    shader_.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });

    shader_.set_uniform_f4("diffuse_color", { ctx.color_diffuse, 4 });
    shader_.set_uniform_f1("shininess", ctx.shininess);

    shader_.set_texture("texture_diffuse", &ctx.texture_diffuse);
    shader_.set_texture("texture_specular", &ctx.texture_specular);

    for (auto i = 0; i < ctx.bone_transforms.size(); i++)
    {
        const auto& per_bone_final_transform = ctx.bone_transforms.at(i);
        const auto uniform_name = "global_bone_transform[" + std::to_string(i) + "]";
        shader_.set_uniform_mat_f4(uniform_name, { glm::value_ptr(per_bone_final_transform), sizeof(per_bone_final_transform) / sizeof(float) });
    }

    geometry.bind();
    geometry.draw(Geometry::Mode::eTriangles);
}
