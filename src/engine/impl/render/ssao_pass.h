#pragma once
#include "render_graph.h"
#include "render_scene_data.h"
#include "render_settings.h"
#include "../graphics.h"

#include <glm/glm.hpp>

#include <array>
#include <string_view>

namespace engine
{
// Screen space ambient occlusion: finds out how much ambient light reaches each pixel, using only the depth buffer.
// The idea is explained in SHADOWS_AND_SSAO.md.
//   1. "SSAO"      - occlusion of every pixel, noisy
//   2. "SSAO Blur" - removes the noise (optional)
class SsaoPass
{
public:
    // full_screen_quad: 6 vertices without attributes (see FrameContext::full_screen_quad), has to outlive the pass
    SsaoPass(RenderContext& rdx, const Geometry& full_screen_quad);

    // Declares the passes for one camera. scene_depth has to contain the depth of everything which occludes.
    // Returns the texture with the result (R8, size of scene_depth): 1 - not occluded, 0 - fully occluded.
    // camera has to stay valid until the graph is executed.
    rg::TextureHandle add_passes(rg::Graph& graph, rg::TextureHandle scene_depth, const CameraGpuData& camera, const SsaoSettings& settings, std::string_view name_suffix);

private:
    // std140, has to match SsaoData in ssao_common.h
    struct GpuData
    {
        glm::mat4 projection;
        glm::mat4 inverse_projection;
        std::array<glm::vec4, SsaoSettings::MAX_SAMPLES> kernel;
        float radius;
        float bias;
        float power;
        std::int32_t sample_count;
    };

    struct OcclusionPassData
    {
        rg::TextureHandle depth;
        rg::TextureHandle occlusion;
    };

    struct BlurPassData
    {
        rg::TextureHandle depth;
        rg::TextureHandle occlusion;
        rg::TextureHandle blurred;
    };

    void upload_gpu_data(const CameraGpuData& camera, const SsaoSettings& settings);
    void draw_full_screen(Shader& shader);

    RenderContext& rdx_;
    Shader shader_occlusion_;
    Shader shader_blur_;
    const Geometry& geometry_full_screen_quad_;
    UniformBuffer gpu_data_ubo_;
    // 4x4 random rotations
    Texture2D noise_texture_;
    // random points inside of a unit hemisphere around +Z
    std::array<glm::vec3, SsaoSettings::MAX_SAMPLES> kernel_;
};

}  // namespace engine
