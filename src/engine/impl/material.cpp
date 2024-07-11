#include "material.h"
#include "math_helpers.h"

engine::MaterialStaticGeometryLit::MaterialStaticGeometryLit()
    : shader_(Shader({ "simple_vertex_definitions.h", "simple.vs" }, { "lit_helpers.h", "lit.fs" }))
{
}

void engine::MaterialStaticGeometryLit::draw(const Geometry& geometry, const DrawContext& ctx)
{
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);
    shader_.set_uniform_block("SceneData", &ctx.scene, 1);

    shader_.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });

    shader_.set_uniform_f4("diffuse_color", {ctx.color_diffuse, 4});
    shader_.set_uniform_f1("shininess", ctx.shininess);

    shader_.set_texture("texture_diffuse", &ctx.texture_diffuse);
    shader_.set_texture("texture_specular", &ctx.texture_specular);

    geometry.bind();
    geometry.draw(Geometry::Mode::eTriangles);
}

engine::MaterialSkinnedGeometryLit::MaterialSkinnedGeometryLit()
    : shader_(Shader({ "simple_vertex_definitions.h", "vertex_skinning.vs" }, { "lit_helpers.h", "lit.fs" }))
{

}

void engine::MaterialSkinnedGeometryLit::draw(const Geometry& geometry, const DrawContext& ctx)
{
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);
    shader_.set_uniform_block("SceneData",  &ctx.scene, 1);

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

engine::MaterialSprite::MaterialSprite()
    : shader_(Shader({ "sprite.vs" }, { "sprite.fs" }))
    , empty_vao_plane_(6)
{
}

void engine::MaterialSprite::draw(const DrawContext& ctx)
{
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);

    shader_.set_uniform_f3("world_position", { glm::value_ptr(ctx.world_position), 3 });
    shader_.set_uniform_f3("scale", { glm::value_ptr(ctx.scale), 3 });

    empty_vao_plane_.bind();
    empty_vao_plane_.draw(Geometry::Mode::eTriangles);
}
