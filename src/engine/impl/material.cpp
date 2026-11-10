#include "material.h"
#include "math_helpers.h"
#include "shaders_binding_slots.h"

#include "profiler.h"

engine::MaterialStaticGeometryLit::MaterialStaticGeometryLit()
    : shader_(Shader({ "simple_vertex_definitions.h", "simple.vs" }, { "lit_helpers.h", "shadow_helpers.h", "lit.fs" }))
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
    : shader_(Shader({ "simple_vertex_definitions.h", "vertex_skinning.vs" }, { "lit_helpers.h", "shadow_helpers.h", "lit.fs" }))
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

    shader_.set_uniform_ui("ssbo_base_offset", ctx.skinning_matrices_offset);
    geometry.bind();
    geometry.draw(Geometry::Mode::eTriangles);
}

engine::MaterialStaticGeometryDepthOnly::MaterialStaticGeometryDepthOnly()
    : shader_(Shader({ "simple_vertex_definitions.h", "simple.vs" }, { "depth_only.fs" }))
{
}

void engine::MaterialStaticGeometryDepthOnly::draw(const Geometry& geometry, const DrawContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("MaterialStaticGeometryDepthOnly::draw");
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);
    shader_.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });

    geometry.bind();
    geometry.draw(Geometry::Mode::eTriangles);
}

engine::MaterialSkinnedGeometryDepthOnly::MaterialSkinnedGeometryDepthOnly()
    : shader_(Shader({ "simple_vertex_definitions.h", "vertex_skinning.vs" }, { "depth_only.fs" }))
{
}

void engine::MaterialSkinnedGeometryDepthOnly::draw(const Geometry& geometry, const DrawContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("MaterialSkinnedGeometryDepthOnly::draw");
    shader_.bind();

    shader_.set_uniform_block("CameraData", &ctx.camera, 0);
    shader_.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });
    shader_.set_uniform_ui("ssbo_base_offset", ctx.skinning_matrices_offset);

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

    for (std::size_t i = 0; i < ctx.bone_transforms.size(); i++)
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

namespace
{
void set_terrain_uniforms(engine::Shader& shader, const engine::TerrainDrawContext& ctx)
{
    shader.set_uniform_block("CameraData", &ctx.camera, 0);
    shader.set_uniform_mat_f4("model", { ctx.model_matrix, 16 });
    shader.set_uniform_ui2("sample_count", ctx.sample_count);
    shader.set_uniform_f1("cell_size", ctx.cell_size);
    shader.set_texture_with_sampler("texture_heights", &ctx.heights);
}

void draw_terrain_chunk(engine::Shader& shader, const engine::Geometry& chunk_grid, std::uint32_t origin_x, std::uint32_t origin_z)
{
    const std::uint32_t origin[2] = { origin_x, origin_z };
    shader.set_uniform_ui2("chunk_origin", origin);
    chunk_grid.draw(engine::Geometry::Mode::eTriangles);
}
}  // namespace

engine::MaterialTerrainLit::MaterialTerrainLit()
    : shader_(Shader({ "terrain_definitions.h", "terrain.vs" }, { "lit_helpers.h", "shadow_helpers.h", "lit.fs" }))
    , repeat_sampler_(TextureAddressClampMode::eRepeat)
{
}

void engine::MaterialTerrainLit::begin(const DrawContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("MaterialTerrainLit::begin");
    shader_.bind();
    set_terrain_uniforms(shader_, ctx.terrain);
    shader_.set_uniform_f2("texture_tile_size", ctx.terrain.texture_tile_size);

    assert(ctx.entity_id != ENGINE_INVALID_GAME_OBJECT_ID);
    shader_.set_uniform_ui("entity_id", ctx.entity_id);
    shader_.set_uniform_block("SceneData", &ctx.scene, 1);
    shader_.set_uniform_f4("diffuse_color", { ctx.color_diffuse, 4 });
    shader_.set_uniform_f1("shininess", ctx.shininess);
    shader_.set_texture_with_sampler("texture_diffuse", &ctx.texture_diffuse);
    shader_.set_texture_with_sampler("texture_specular", &ctx.texture_specular);
    repeat_sampler_.bind(static_cast<std::uint32_t>(ShaderBindingSlot::TEXTURE_DIFFUSE));
    repeat_sampler_.bind(static_cast<std::uint32_t>(ShaderBindingSlot::TEXTURE_SPECULAR));
}

void engine::MaterialTerrainLit::draw_chunk(const Geometry& chunk_grid, std::uint32_t origin_x, std::uint32_t origin_z)
{
    draw_terrain_chunk(shader_, chunk_grid, origin_x, origin_z);
}

void engine::MaterialTerrainLit::end()
{
    Sampler::unbind(static_cast<std::uint32_t>(ShaderBindingSlot::TEXTURE_DIFFUSE));
    Sampler::unbind(static_cast<std::uint32_t>(ShaderBindingSlot::TEXTURE_SPECULAR));
}

engine::MaterialTerrainDepthOnly::MaterialTerrainDepthOnly()
    : shader_(Shader({ "terrain_definitions.h", "terrain.vs" }, { "depth_only.fs" }))
{
}

void engine::MaterialTerrainDepthOnly::begin(const TerrainDrawContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("MaterialTerrainDepthOnly::begin");
    shader_.bind();
    set_terrain_uniforms(shader_, ctx);
}

void engine::MaterialTerrainDepthOnly::draw_chunk(const Geometry& chunk_grid, std::uint32_t origin_x, std::uint32_t origin_z)
{
    draw_terrain_chunk(shader_, chunk_grid, origin_x, origin_z);
}
