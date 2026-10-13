#include "ssao_pass.h"
#include "../profiler.h"

#include <fmt/format.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cstdint>
#include <random>

namespace
{
constexpr std::uint32_t NOISE_SIZE = 4;  // ssao.fs and ssao_blur.fs assume 4x4

// Random unit vectors in the XY plane, packed to <0; 255>.
std::array<std::uint8_t, NOISE_SIZE * NOISE_SIZE * 4> generate_noise(std::mt19937& generator)
{
    std::uniform_real_distribution<float> angle_distribution(0.0f, glm::two_pi<float>());
    std::array<std::uint8_t, NOISE_SIZE * NOISE_SIZE * 4> noise{};
    for (std::size_t i = 0; i < NOISE_SIZE * NOISE_SIZE; i++)
    {
        const auto angle = angle_distribution(generator);
        noise[i * 4 + 0] = static_cast<std::uint8_t>((std::cos(angle) * 0.5f + 0.5f) * 255.0f);
        noise[i * 4 + 1] = static_cast<std::uint8_t>((std::sin(angle) * 0.5f + 0.5f) * 255.0f);
        noise[i * 4 + 2] = 0;
        noise[i * 4 + 3] = 255;
    }
    return noise;
}
}  // namespace

engine::SsaoPass::SsaoPass(RenderContext& rdx)
    : rdx_(rdx)
    , shader_occlusion_({ "full_screen_quad.vs" }, { "ssao_common.h", "ssao.fs" })
    , shader_blur_({ "full_screen_quad.vs" }, { "ssao_common.h", "ssao_blur.fs" })
    , geometry_full_screen_quad_(6)
    , gpu_data_ubo_(sizeof(GpuData))
{
    // fixed seed: the result looks the same on every run
    std::mt19937 generator(1234);
    noise_texture_ = Texture2D(NOISE_SIZE, NOISE_SIZE, false, generate_noise(generator).data(), DataLayout::eRGBA_U8, TextureAddressClampMode::eClampToEdge);

    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    for (auto& sample : kernel_)
    {
        // random direction in the hemisphere, random distance from the center
        const glm::vec3 direction(distribution(generator) * 2.0f - 1.0f, distribution(generator) * 2.0f - 1.0f, distribution(generator));
        sample = glm::normalize(direction + glm::vec3(0.0f, 0.0f, 1e-3f)) * distribution(generator);
    }
}

void engine::SsaoPass::upload_gpu_data(const CameraGpuData& camera, const SsaoSettings& settings)
{
    const auto sample_count = std::clamp<std::uint32_t>(settings.sample_count, 1, SsaoSettings::MAX_SAMPLES);

    BufferMapContext<GpuData, UniformBuffer> gpu_data(gpu_data_ubo_, false, true);
    gpu_data.data->projection = camera.projection;
    gpu_data.data->inverse_projection = glm::inverse(camera.projection);
    for (std::uint32_t i = 0; i < sample_count; i++)
    {
        // Samples are pulled towards the center: occluders close to the surface matter more than the distant ones.
        const auto t = static_cast<float>(i) / static_cast<float>(sample_count);
        gpu_data.data->kernel[i] = glm::vec4(kernel_[i] * glm::mix(0.1f, 1.0f, t * t), 0.0f);
    }
    gpu_data.data->radius = settings.radius;
    gpu_data.data->bias = settings.bias;
    gpu_data.data->power = settings.power;
    gpu_data.data->sample_count = static_cast<std::int32_t>(sample_count);
}

void engine::SsaoPass::draw_full_screen(Shader& shader)
{
    rdx_.set_depth_test(false);
    rdx_.set_cull_face(false);
    shader.set_uniform_block("SsaoData", &gpu_data_ubo_, 0);
    geometry_full_screen_quad_.bind();
    geometry_full_screen_quad_.draw(Geometry::Mode::eTriangles);
}

engine::rg::TextureHandle engine::SsaoPass::add_passes(rg::Graph& graph, rg::TextureHandle scene_depth, const CameraGpuData& camera, const SsaoSettings& settings, std::string_view name_suffix)
{
    const auto& occlusion_pass = graph.add_pass<OcclusionPassData>(fmt::format("SSAO{}", name_suffix),
        [&](rg::Builder& builder, OcclusionPassData& data)
        {
            const auto depth_desc = builder.desc(scene_depth);
            data.depth = builder.read(scene_depth);
            data.occlusion = builder.write_color(0, builder.create_texture("AmbientOcclusionNoisy", rg::TextureDesc{ depth_desc.width, depth_desc.height, DataLayout::eR_U8 }), rg::LoadOp::eDontCare);
        },
        [this, camera_ptr = &camera, settings](const OcclusionPassData& data, rg::PassContext& ctx)
        {
            ENGINE_PROFILE_SECTION_N("SsaoPass::occlusion");
            upload_gpu_data(*camera_ptr, settings);
            shader_occlusion_.bind();
            shader_occlusion_.set_texture_with_sampler("texture_depth", &ctx.texture(data.depth));
            shader_occlusion_.set_texture_with_sampler("texture_noise", &noise_texture_);
            draw_full_screen(shader_occlusion_);
        });

    if (!settings.blur)
    {
        return occlusion_pass.occlusion;
    }

    const auto& blur_pass = graph.add_pass<BlurPassData>(fmt::format("SSAO Blur{}", name_suffix),
        [&](rg::Builder& builder, BlurPassData& data)
        {
            data.depth = builder.read(scene_depth);
            data.occlusion = builder.read(occlusion_pass.occlusion);
            const auto occlusion_desc = builder.desc(occlusion_pass.occlusion);
            data.blurred = builder.write_color(0, builder.create_texture("AmbientOcclusion", occlusion_desc), rg::LoadOp::eDontCare);
        },
        [this, camera_ptr = &camera, settings](const BlurPassData& data, rg::PassContext& ctx)
        {
            ENGINE_PROFILE_SECTION_N("SsaoPass::blur");
            upload_gpu_data(*camera_ptr, settings);
            shader_blur_.bind();
            shader_blur_.set_texture_with_sampler("texture_depth", &ctx.texture(data.depth));
            shader_blur_.set_texture_with_sampler("texture_occlusion", &ctx.texture(data.occlusion));
            draw_full_screen(shader_blur_);
        });
    return blur_pass.blurred;
}
