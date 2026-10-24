#pragma once
#include "render_graph.h"
#include "render_scene_data.h"
#include "render_settings.h"
#include "../graphics.h"

#include <glm/glm.hpp>

namespace engine
{
// Outlines of the low resolution "pixel art" look: darkens silhouettes and brightens creases, one scene pixel wide.
// The idea is explained in PIXEL_ART.md.
//   "Pixel Outline" - reads scene color, depth and normals, writes a new scene color
class PixelOutlinePass
{
public:
    // full_screen_quad: 6 vertices without attributes (see FrameContext::full_screen_quad), has to outlive the pass
    PixelOutlinePass(RenderContext& rdx, const Geometry& full_screen_quad);

    // Returns the outlined scene color (same size and layout as scene_color).
    // camera: the camera scene_depth was rendered with, has to stay valid until the graph is executed.
    rg::TextureHandle add_pass(rg::Graph& graph, rg::TextureHandle scene_color, rg::TextureHandle scene_depth, rg::TextureHandle scene_normals,
        const CameraGpuData& camera, const PixelArtSettings& settings);

private:
    struct PassData
    {
        rg::TextureHandle color;
        rg::TextureHandle depth;
        rg::TextureHandle normals;
        rg::TextureHandle result;
    };

    RenderContext& rdx_;
    Shader shader_;
    const Geometry& geometry_full_screen_quad_;
};

}  // namespace engine
