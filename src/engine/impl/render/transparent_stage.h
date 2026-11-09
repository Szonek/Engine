#pragma once
#include "render_graph.h"

#include <fmt/format.h>

#include <string_view>
#include <utility>

namespace engine
{
// What the Scene Depth Copy pass reads and writes.
struct DepthCopyPassData
{
    rg::TextureHandle scene_depth;
    rg::TextureHandle depth_copy;
};

// What the Transparent pass samples: the copy of the scene's depth (soft particles; invalid without one) and the
// directional light's shadow map (lit particles; invalid without shadows).
struct TransparentPassData
{
    rg::TextureHandle depth_copy;
    rg::TextureHandle shadow_map;
};

// The Transparent stage of a view: blended geometry (particles for now; later water, glass) drawn over SceneColor after
// the opaque passes (Forward, Sprites) and before debug lines and the post-process features, depth tested against
// SceneDepth without writing it (the draw callback sets the blending and turns depth writes off). It loads both
// targets, so it never makes the opaque passes before it unnecessary, and it writes no entity ids (transparent things
// can't be picked).
// Soft particles fade where they near the geometry behind them, which needs the scene's depth while SceneDepth is the
// bound depth attachment (sampling it would be a feedback loop): with `needs_depth_copy` a Scene Depth Copy pass first
// copies it into an R32F texture of the same size, which the Transparent pass samples; without it nothing is copied.
// GL-free, unit tested in render_graph_tests.cpp.
//   copy: void(const DepthCopyPassData&, rg::PassContext&)
//   draw: void(const TransparentPassData&, rg::PassContext&)
template<typename CopyFn, typename DrawFn>
void add_transparent_passes(rg::Graph& graph, std::string_view name_suffix, rg::TextureHandle scene_color, rg::TextureHandle scene_depth,
    rg::TextureHandle shadow_map, bool needs_depth_copy, CopyFn&& copy, DrawFn&& draw)
{
    rg::TextureHandle depth_copy;
    if (needs_depth_copy)
    {
        depth_copy = graph.add_pass<DepthCopyPassData>(fmt::format("Scene Depth Copy{}", name_suffix),
            [&](rg::Builder& builder, DepthCopyPassData& data)
            {
                const auto desc = builder.desc(scene_depth);
                data.scene_depth = builder.read(scene_depth);
                data.depth_copy = builder.write_color(0, builder.create_texture("SceneDepthCopy", rg::TextureDesc{ desc.width, desc.height, DataLayout::eR_FP32 }),
                    rg::LoadOp::eDontCare);
            },
            std::forward<CopyFn>(copy)).depth_copy;
    }
    graph.add_pass<TransparentPassData>(fmt::format("Transparent{}", name_suffix),
        [&](rg::Builder& builder, TransparentPassData& data)
        {
            if (depth_copy.is_valid())
            {
                data.depth_copy = builder.read(depth_copy);
            }
            if (shadow_map.is_valid())
            {
                data.shadow_map = builder.read(shadow_map);
            }
            builder.write_color(0, scene_color, rg::LoadOp::eLoad);
            builder.write_depth(scene_depth, rg::LoadOp::eLoad);
        },
        std::forward<DrawFn>(draw));
}
}  // namespace engine
