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
    // size of the scene image: the window, or the editor's scene panel
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    rg::TextureHandle scene_color;   // what is displayed on the screen (RGBA8)
    rg::TextureHandle entity_id;     // entity id of the geometry under each pixel (R32UI), used for picking
    rg::TextureHandle scene_depth;
    rg::TextureHandle scene_output;  // the presented scene image: the backbuffer, or the texture the editor shows in its scene panel
    rg::TextureHandle backbuffer;    // the screen, window sized

    std::span<const FrameView> views;

    // 6 vertices without attributes: two triangles covering the screen, positions are generated in the vertex shader
    // from gl_VertexID (see full_screen_quad.vs)
    const Geometry* full_screen_quad = nullptr;
};

enum class RenderStage
{
    // Modifies scene_color before it's presented, i.e. outline, bloom, tone mapping...
    ePostProcess,
    // Draws on top of the presented scene image (FrameContext::scene_output), i.e. game UI.
    eOverlay,
    // Draws to the backbuffer once the scene image is complete, i.e. the editor, which shows that image in a panel.
    eScreen
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

// Adds a pass which draws on top of what is already in target (FrameContext::scene_output or FrameContext::backbuffer).
// execute: void(rg::PassContext&)
template<typename ExecuteFn>
void add_overlay_pass(FrameContext& frame, std::string_view name, rg::TextureHandle target, ExecuteFn&& execute)
{
    frame.graph.add_pass(name,
        [target](rg::Builder& builder, rg::NoPassData&)
        {
            builder.write_color(0, target, rg::LoadOp::eLoad);
        },
        [fn = std::forward<ExecuteFn>(execute)](const rg::NoPassData&, rg::PassContext& ctx) mutable
        {
            fn(ctx);
        });
}

}  // namespace engine
