#include "material.h"
#include "math_helpers.h"

#include "profiler.h"

engine::MaterialStaticGeometryLit::MaterialStaticGeometryLit()
    : shader_(Shader({ "simple_vertex_definitions.h", "simple.vs" }, { "lit_helpers.h", "lit.fs" }))
{
}

void engine::MaterialStaticGeometryLit::draw(const Geometry& geometry, const DrawContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("MaterialStaticGeometryLit::draw");
    shader_.bind();
    
    assert(ctx.entity_id != ENGINE_INVALID_GAME_OBJECT_ID);
    shader_.set_uniform_ui("entity_id", ctx.entity_id);

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);
    shader_.set_uniform_block("SceneData", &ctx.scene, 1);

    assert(ctx.model_matrix != nullptr);
    shader_.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });

    shader_.set_uniform_f4("diffuse_color", {ctx.color_diffuse, 4});
    shader_.set_uniform_f1("shininess", ctx.shininess);

    shader_.set_texture_with_sampler("texture_diffuse", &ctx.texture_diffuse);
    shader_.set_texture_with_sampler("texture_specular", &ctx.texture_specular);

    geometry.bind();
    geometry.draw(Geometry::Mode::eTriangles);
}

engine::MaterialSkinnedGeometryLit::MaterialSkinnedGeometryLit()
    : shader_(Shader({ "simple_vertex_definitions.h", "vertex_skinning.vs" }, { "lit_helpers.h", "lit.fs" }))
{

}

void engine::MaterialSkinnedGeometryLit::draw(const Geometry& geometry, const DrawContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("MaterialSkinnedGeometryLit::draw");
    shader_.bind();

    assert(ctx.entity_id != ENGINE_INVALID_GAME_OBJECT_ID);
    shader_.set_uniform_ui("entity_id", ctx.entity_id);

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);
    shader_.set_uniform_block("SceneData",  &ctx.scene, 1);

    assert(ctx.model_matrix != nullptr);
    shader_.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });

    shader_.set_uniform_f4("diffuse_color", { ctx.color_diffuse, 4 });
    shader_.set_uniform_f1("shininess", ctx.shininess);

    shader_.set_texture_with_sampler("texture_diffuse", &ctx.texture_diffuse);
    shader_.set_texture_with_sampler("texture_specular", &ctx.texture_specular);

    {
        ENGINE_PROFILE_SECTION_N("MaterialSkinnedGeometryLit::draw::UpdatePerBoneUniform");
        for (auto i = 0; i < ctx.bone_transforms.size(); i++)
        {
            const auto& per_bone_final_transform = ctx.bone_transforms[i];
            const auto uniform_name = "global_bone_transform[" + std::to_string(i) + "]";
            shader_.set_uniform_mat_f4(uniform_name, { glm::value_ptr(per_bone_final_transform), sizeof(per_bone_final_transform) / sizeof(float) });
        }
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
    ENGINE_PROFILE_SECTION_N("MaterialSprite::draw");
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);

    shader_.set_uniform_f3("world_position", { glm::value_ptr(ctx.world_position), 3 });
    shader_.set_uniform_f3("scale", { glm::value_ptr(ctx.scale), 3 });

    empty_vao_plane_.bind();
    empty_vao_plane_.draw(Geometry::Mode::eTriangles);
}

engine::MaterialSkinnedGeometryUnlit::MaterialSkinnedGeometryUnlit()
    : shader_(Shader({ "simple_vertex_definitions.h", "vertex_skinning.vs" }, { "unlit.fs" }))
{
}

void engine::MaterialSkinnedGeometryUnlit::draw(const Geometry& geometry, const DrawContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("MaterialSkinnedGeometryUnlit::draw");
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);

    shader_.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });
    shader_.set_uniform_f4("diffuse_color", { ctx.color_diffuse, 4 });
    shader_.set_texture_with_sampler("texture_diffuse", &ctx.texture_diffuse);

    for (auto i = 0; i < ctx.bone_transforms.size(); i++)
    {
        const auto& per_bone_final_transform = ctx.bone_transforms.at(i);
        const auto uniform_name = "global_bone_transform[" + std::to_string(i) + "]";
        shader_.set_uniform_mat_f4(uniform_name, { glm::value_ptr(per_bone_final_transform), sizeof(per_bone_final_transform) / sizeof(float) });
    }

    geometry.bind();
    geometry.draw(Geometry::Mode::eTriangles);
}

engine::MaterialStaticGeometryUnlit::MaterialStaticGeometryUnlit()
    : shader_(Shader({ "simple_vertex_definitions.h", "simple.vs" }, { "unlit.fs" }))
{
}

void engine::MaterialStaticGeometryUnlit::draw(const Geometry& geometry, const DrawContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("MaterialStaticGeometryUnlit::draw");
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);
    shader_.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });

    shader_.set_uniform_f4("diffuse_color", { ctx.color_diffuse, 4 });
    shader_.set_texture_with_sampler("texture_diffuse", &ctx.texture_diffuse);

    geometry.bind();
    geometry.draw(Geometry::Mode::eTriangles);
}
