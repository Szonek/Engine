#pragma once
#include "render_graph.h"

#include <string_view>
#include <utility>

namespace engine
{
// The Transparent stage of a view: blended geometry (particles for now; later water, glass) drawn over SceneColor after
// the opaque passes (Forward, Sprites) and before debug lines and the post-process features, depth tested against
// SceneDepth without writing it (the execute callback sets the blending and turns depth writes off). It loads both
// targets, so it never makes the opaque passes before it unnecessary, and it writes no entity ids (transparent things
// can't be picked). GL-free, unit tested in render_graph_tests.cpp.
template<typename ExecuteFn>
void add_transparent_pass(rg::Graph& graph, std::string_view name, rg::TextureHandle scene_color, rg::TextureHandle scene_depth, ExecuteFn&& execute)
{
    graph.add_pass(name,
        [&](rg::Builder& builder, rg::NoPassData&)
        {
            builder.write_color(0, scene_color, rg::LoadOp::eLoad);
            builder.write_depth(scene_depth, rg::LoadOp::eLoad);
        },
        [execute = std::forward<ExecuteFn>(execute)](const rg::NoPassData&, rg::PassContext& ctx) { execute(ctx); });
}
}  // namespace engine
