#include "grid_feature.h"
#include "../profiler.h"

#include <glm/gtc/type_ptr.hpp>

engine::GridFeature::GridFeature(RenderContext& rdx)
    : rdx_(rdx)
    , shader_({ "editor_grid.vs" }, { "editor_grid.fs" })
    , geometry_full_screen_triangle_(3)
{
}

void engine::GridFeature::add_passes(FrameContext& frame)
{
    if (frame.views.empty())
    {
        return;
    }

    frame.graph.add_pass("Grid",
        [&](rg::Builder& builder, rg::NoPassData&)
        {
            builder.write_color(0, frame.scene_color, rg::LoadOp::eLoad);
            builder.write_depth(frame.scene_depth, rg::LoadOp::eLoad);
        },
        [this, &frame](const rg::NoPassData&, rg::PassContext&)
        {
            ENGINE_PROFILE_SECTION_N("GridFeature::grid");
            // depth is only tested: the grid is transparent, it must not hide anything drawn later
            rdx_.set_depth_write(false);
            rdx_.set_depth_func(RenderContext::DepthFunc::eLessOrEqual);
            rdx_.set_cull_face(false);
            // alpha of the scene color is left untouched
            rdx_.set_blend_mode(true,
                RenderContext::BlendFactor::eSrcAlpha, RenderContext::BlendFactor::eOneMinusSrcAlpha,
                RenderContext::BlendFactor::eZero, RenderContext::BlendFactor::eOne);

            shader_.bind();
            geometry_full_screen_triangle_.bind();
            for (const auto& view : frame.views)
            {
                const auto view_projection = view.camera->projection * view.camera->view;
                const auto inverse_view_projection = glm::inverse(view_projection);
                shader_.set_uniform_mat_f4("view_projection", { glm::value_ptr(view_projection), 16 });
                shader_.set_uniform_mat_f4("inverse_view_projection", { glm::value_ptr(inverse_view_projection), 16 });
                geometry_full_screen_triangle_.draw(Geometry::Mode::eTriangles);
            }
        });
}
