#pragma once
#include "render_graph.h"

#include <string>
#include <string_view>
#include <utility>

namespace engine
{
// What the Transparent pass of a view reads besides the targets it draws into.
struct TransparentInputs
{
    // the view's shadow map, for lit particles; invalid when the view has none
    rg::TextureHandle shadow_map;
    // soft particles fade against the opaque scene's depth: SceneDepth is copied first (a texture can't be sampled
    // while it is the depth attachment the pass tests against), only when something drawn needs it
    bool needs_scene_depth = false;
};

// The textures the Transparent pass's execute callback gets (invalid when not asked for).
struct TransparentPassData
{
    // SceneDepth as it was after the opaque passes, R_FP32 window space depth (0..1)
    rg::TextureHandle scene_depth_copy;
    rg::TextureHandle shadow_map;
};

// The Transparent stage of a view: blended geometry (particles for now; later water, glass) drawn over SceneColor after
// the opaque passes (Forward, Sprites) and before debug lines and the post-process features, depth tested against
// SceneDepth without writing it (the execute callback sets the blending and turns depth writes off). It loads both
// targets, so it never makes the opaque passes before it unnecessary, and it writes no entity ids (transparent things
// can't be picked). With `needs_scene_depth` a "<name> Depth Copy" pass before it copies SceneDepth into a texture
// it samples (`copy_depth(ctx, scene_depth)` draws it: a full screen pass writing the depth into its one colour target).
// GL-free, unit tested in render_graph_tests.cpp.
template<typename CopyDepthFn, typename ExecuteFn>
void add_transparent_pass(rg::Graph& graph, std::string_view name, rg::TextureHandle scene_color, rg::TextureHandle scene_depth,
    const TransparentInputs& inputs, CopyDepthFn&& copy_depth, ExecuteFn&& execute)
{
    rg::TextureHandle depth_copy;
    if (inputs.needs_scene_depth)
    {
        struct CopyData
        {
            rg::TextureHandle source;
            rg::TextureHandle target;
        };
        depth_copy = graph.add_pass<CopyData>(std::string(name) + " Depth Copy",
            [&](rg::Builder& builder, CopyData& data)
            {
                data.source = builder.read(scene_depth);
                const auto& desc = builder.desc(scene_depth);
                data.target = builder.write_color(0, builder.create_texture("SceneDepthCopy", rg::TextureDesc{ desc.width, desc.height, DataLayout::eR_FP32 }), rg::LoadOp::eDontCare);
            },
            [copy_depth = std::forward<CopyDepthFn>(copy_depth)](const CopyData& data, rg::PassContext& ctx) { copy_depth(ctx, data.source); })
            .target;
    }

    graph.add_pass<TransparentPassData>(name,
        [&](rg::Builder& builder, TransparentPassData& data)
        {
            if (depth_copy.is_valid())
            {
                data.scene_depth_copy = builder.read(depth_copy);
            }
            if (inputs.shadow_map.is_valid())
            {
                data.shadow_map = builder.read(inputs.shadow_map);
            }
            builder.write_color(0, scene_color, rg::LoadOp::eLoad);
            builder.write_depth(scene_depth, rg::LoadOp::eLoad);
        },
        [execute = std::forward<ExecuteFn>(execute)](const TransparentPassData& data, rg::PassContext& ctx) { execute(ctx, data); });
}
}  // namespace engine
