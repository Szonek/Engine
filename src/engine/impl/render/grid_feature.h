#pragma once
#include "render_feature.h"
#include "../graphics.h"

namespace engine
{
// Infinite, anti-aliased grid on the XZ plane (y = 0) blended into the scene color. Used by the editor.
// One full screen pass per camera: the fragment shader (editor_grid.fs) intersects camera rays with the plane,
// and is depth tested against the scene depth, so geometry in front of the grid hides it.
class GridFeature final : public RenderFeature
{
public:
    explicit GridFeature(RenderContext& rdx);

    std::string_view get_name() const override { return "Grid"; }
    void add_passes(FrameContext& frame) override;

private:
    RenderContext& rdx_;
    Shader shader_;
};

}  // namespace engine
