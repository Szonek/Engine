#pragma once
#include "render_feature.h"
#include "../graphics.h"
#include "../material.h"

namespace engine
{
// Draws an outline around geometry listed in SceneRenderData::outline_items (i.e. selected objects in the editor).
//   1. "Outline Mask"           - geometry is rendered in white to a transient texture
//   2. "Outline Edge Detection" - compute shader (sobel) finds edges of the mask and adds them to the scene color
class OutlineFeature final : public RenderFeature
{
public:
    OutlineFeature();

    std::string_view get_name() const override { return "Outline"; }
    void add_passes(FrameContext& frame) override;

private:
    struct MaskPassData
    {
        rg::TextureHandle mask;
    };

    struct EdgeDetectionPassData
    {
        rg::TextureHandle mask;
        rg::TextureHandle scene_color;
    };

    MaterialStaticGeometryUnlit material_static_geometry_unlit_;
    ComputeShader compute_shader_edge_detection_;
    Texture2D white_texture_;
};

}  // namespace engine
