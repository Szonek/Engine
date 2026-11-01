#include "pixel_outline_pass.h"
#include "../profiler.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

engine::PixelOutlinePass::PixelOutlinePass(RenderContext& rdx, const Geometry& full_screen_quad)
    : rdx_(rdx)
    , shader_({ "full_screen_quad.vs" }, { "pixel_outline.fs" })
    , geometry_full_screen_quad_(full_screen_quad)
{
}

engine::rg::TextureHandle engine::PixelOutlinePass::add_pass(rg::Graph& graph, rg::TextureHandle scene_color, rg::TextureHandle scene_depth, rg::TextureHandle scene_normals,
    const CameraGpuData& camera, const PixelArtSettings& settings)
{
    return graph.add_pass<PassData>("Pixel Outline",
        [&](rg::Builder& builder, PassData& data)
        {
            data.color = builder.read(scene_color);
            data.depth = builder.read(scene_depth);
            data.normals = builder.read(scene_normals);
            data.result = builder.write_color(0, builder.create_texture("SceneColorOutlined", builder.desc(scene_color)), rg::LoadOp::eDontCare);
        },
        [this, camera_ptr = &camera, settings](const PassData& data, rg::PassContext& ctx)
        {
            ENGINE_PROFILE_SECTION_N("PixelOutlinePass");
            rdx_.set_depth_test(false);
            rdx_.set_cull_face(false);
            const auto inverse_projection = glm::inverse(camera_ptr->projection);
            shader_.bind();
            shader_.set_texture_with_sampler("scene_color", &ctx.texture(data.color));
            shader_.set_texture_with_sampler("scene_depth", &ctx.texture(data.depth));
            shader_.set_texture_with_sampler("scene_normals", &ctx.texture(data.normals));
            shader_.set_uniform_mat_f4("inverse_projection", { glm::value_ptr(inverse_projection), 16 });
            shader_.set_uniform_f1("outline_strength", std::clamp(settings.outline_strength, 0.0f, 1.0f));
            shader_.set_uniform_f1("highlight_strength", std::max(settings.highlight_strength, 0.0f));
            shader_.set_uniform_f1("depth_threshold", std::max(settings.outline_depth_threshold, 0.0f));
            shader_.set_uniform_f1("normal_threshold", std::max(settings.highlight_normal_threshold, 0.0f));
            geometry_full_screen_quad_.bind();
            geometry_full_screen_quad_.draw(Geometry::Mode::eTriangles);
        }).result;
}
