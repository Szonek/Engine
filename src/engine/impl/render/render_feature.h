#pragma once
#include "render_graph.h"
#include "render_scene_data.h"

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace engine
{
class RenderContext;
class UniformBuffer;
class ShaderStorageBuffer;
class Geometry;

// A camera of a scene together with GPU buffers prepared by the renderer for this frame.
struct FrameView
{
    const SceneRenderData* scene = nullptr;
    const CameraGpuData* camera = nullptr;
    const UniformBuffer* camera_ubo = nullptr;
    const UniformBuffer* scene_ubo = nullptr;
    const ShaderStorageBuffer* lights_ssbo = nullptr;
    const ShaderStorageBuffer* debug_lines_ssbo = nullptr;
    // bone matrices of all skinned meshes of the scene, see SkinnedMeshDrawItem::skinning_matrices_offset
    const ShaderStorageBuffer* skinning_ssbo = nullptr;
    // shadows and ambient occlusion parameters (ViewLightingGpuData)
    const UniformBuffer* lighting_ubo = nullptr;
    // camera of the directional light (CameraGpuData), valid only if has_shadow is set
    const UniformBuffer* shadow_camera_ubo = nullptr;
    bool has_shadow = false;
};

// Everything a feature needs to declare its passes.
// It lives only during Renderer::render_frame(), which is also when pass execute callbacks run, so callbacks may capture it by reference.
struct FrameContext
{
    rg::Graph& graph;
    RenderContext& rdx;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    rg::TextureHandle scene_color;  // what is displayed on the screen (RGBA8)
    rg::TextureHandle entity_id;    // entity id of the geometry under each pixel (R32UI), used for picking
    rg::TextureHandle scene_depth;
    rg::TextureHandle backbuffer;   // the screen

    std::span<const FrameView> views;

    // 6 vertices without attributes: two triangles covering the screen, positions are generated in the vertex shader
    // from gl_VertexID (see full_screen_quad.vs)
    const Geometry* full_screen_quad = nullptr;
};

enum class RenderStage
{
    // Modifies scene_color before it's presented, i.e. outline, bloom, tone mapping...
    ePostProcess,
    // Draws on top of the presented image directly to the backbuffer, i.e. UI, editor widgets.
    eOverlay
};

// An independent piece of rendering (effect, overlay, ...) which can be plugged into the Renderer without changing it.
class RenderFeature
{
public:
    virtual ~RenderFeature() = default;

    virtual std::string_view get_name() const = 0;
    // Declare passes in frame.graph. Called every frame when the feature is enabled.
    virtual void add_passes(FrameContext& frame) = 0;

    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

private:
    bool enabled_ = true;
};

// Feature built from a function, handy for small things.
class CallbackRenderFeature final : public RenderFeature
{
public:
    CallbackRenderFeature(std::string name, std::function<void(FrameContext&)> add_passes_fn)
        : name_(std::move(name))
        , add_passes_fn_(std::move(add_passes_fn))
    {
    }

    std::string_view get_name() const override { return name_; }
    void add_passes(FrameContext& frame) override { add_passes_fn_(frame); }

private:
    std::string name_;
    std::function<void(FrameContext&)> add_passes_fn_;
};

// Adds a pass which draws directly to the backbuffer on top of what is already there.
// execute: void(rg::PassContext&)
template<typename ExecuteFn>
void add_backbuffer_overlay_pass(FrameContext& frame, std::string_view name, ExecuteFn&& execute)
{
    frame.graph.add_pass(name,
        [&frame](rg::Builder& builder, rg::NoPassData&)
        {
            builder.write_color(0, frame.backbuffer, rg::LoadOp::eLoad);
        },
        [fn = std::forward<ExecuteFn>(execute)](const rg::NoPassData&, rg::PassContext& ctx) mutable
        {
            fn(ctx);
        });
}

}  // namespace engine
