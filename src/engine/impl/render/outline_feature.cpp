#include "outline_feature.h"
#include "../profiler.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace
{
// must match local_size of sobel_edge_detection.cs
constexpr std::uint32_t EDGE_DETECTION_GROUP_SIZE_X = 32;
constexpr std::uint32_t EDGE_DETECTION_GROUP_SIZE_Y = 4;

constexpr std::uint32_t divide_round_up(std::uint32_t value, std::uint32_t divisor)
{
    return (value + divisor - 1) / divisor;
}

constexpr std::array<std::uint8_t, 4> WHITE_PIXEL = { 255, 255, 255, 255 };
}  // namespace

engine::OutlineFeature::OutlineFeature()
    : compute_shader_edge_detection_({ "sobel_edge_detection.cs" })
    , white_texture_(1, 1, false, WHITE_PIXEL.data(), DataLayout::eRGBA_U8, TextureAddressClampMode::eClampToEdge)
{
}

void engine::OutlineFeature::add_passes(FrameContext& frame)
{
    const bool has_outlined_geometry = std::ranges::any_of(frame.views, [](const FrameView& view) { return !view.scene->outline_items.empty(); });
    if (!has_outlined_geometry)
    {
        return;
    }

    const auto& mask_pass = frame.graph.add_pass<MaskPassData>("Outline Mask",
        [&](rg::Builder& builder, MaskPassData& data)
        {
            const auto mask = builder.create_texture("OutlineMask", rg::TextureDesc{ frame.width, frame.height, DataLayout::eRGBA_U8 });
            data.mask = builder.write_color(0, mask, rg::LoadOp::eClear, rg::ClearValue{ .color = { 0.0f, 0.0f, 0.0f, 1.0f } });
        },
        [this, &frame](const MaskPassData&, rg::PassContext&)
        {
            ENGINE_PROFILE_SECTION_N("OutlineFeature::mask");
            const glm::vec4 white(1.0f);
            for (const auto& view : frame.views)
            {
                for (const auto& item : view.scene->outline_items)
                {
                    const MaterialStaticGeometryUnlit::DrawContext ctx{
                        .camera = *view.camera_ubo,
                        .model_matrix = glm::value_ptr(item.model),
                        .color_diffuse = glm::value_ptr(white),
                        .texture_diffuse = white_texture_ };
                    material_static_geometry_unlit_.draw(*item.geometry, ctx);
                }
            }
        });

    frame.graph.add_pass<EdgeDetectionPassData>("Outline Edge Detection",
        [&](rg::Builder& builder, EdgeDetectionPassData& data)
        {
            data.mask = builder.read_image(mask_pass.mask);
            data.scene_color = builder.write_image(frame.scene_color);
        },
        [this](const EdgeDetectionPassData& data, rg::PassContext& ctx)
        {
            ENGINE_PROFILE_SECTION_N("OutlineFeature::edge_detection");
            const auto& color_desc = ctx.desc(data.scene_color);
            compute_shader_edge_detection_.bind();
            compute_shader_edge_detection_.set_texture("img_in", &ctx.texture(data.mask), AccessType::eReadOnly, DataLayout::eRGBA_U8);
            compute_shader_edge_detection_.set_texture("img_out", &ctx.texture(data.scene_color), AccessType::eReadWrite, DataLayout::eRGBA_U8);
            compute_shader_edge_detection_.dispatch(
                divide_round_up(color_desc.width, EDGE_DETECTION_GROUP_SIZE_X),
                divide_round_up(color_desc.height, EDGE_DETECTION_GROUP_SIZE_Y),
                1);
        });
}
