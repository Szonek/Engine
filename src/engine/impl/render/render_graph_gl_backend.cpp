#include "render_graph_gl_backend.h"
#include "../logger.h"

#include <algorithm>

namespace
{
constexpr std::uint64_t POOL_UNUSED_FRAMES_BEFORE_RELEASE = 3;
}

engine::GlGraphBackend::GlGraphBackend(RenderContext& rdx)
    : rdx_(rdx)
{
}

std::shared_ptr<engine::Texture2D> engine::GlGraphBackend::acquire_texture(const rg::TextureDesc& desc, std::string_view /*name*/)
{
    // use_count == 1: only the pool references it, nobody (graph, extracted copy) uses it right now
    for (auto& entry : pool_)
    {
        if (entry.texture.use_count() == 1 && entry.desc == desc)
        {
            entry.last_used_frame = frame_index_;
            return entry.texture;
        }
    }

    auto texture = std::make_shared<Texture2D>(desc.width, desc.height, false, nullptr, desc.layout, TextureAddressClampMode::eClampToEdge);
    pool_.push_back(PoolEntry{ desc, texture, frame_index_ });
    return texture;
}

void engine::GlGraphBackend::begin_pass(const rg::PassBeginInfo& info)
{
    rdx_.begin_debug_group(info.name);
    if (!info.is_raster)
    {
        RenderTargetSet::bind_default();
        return;
    }

    const bool is_backbuffer_pass = info.color_attachments.size() == 1 && info.color_attachments.front().is_backbuffer;
    if (is_backbuffer_pass)
    {
        RenderTargetSet::bind_default();
    }
    else
    {
        render_targets_.bind();
        std::array<const Texture2D*, RenderTargetSet::MAX_COLOR_ATTACHMENTS> color_textures{};
        for (const auto& attachment : info.color_attachments)
        {
            color_textures[attachment.slot] = attachment.texture;
        }
        for (std::uint32_t slot = 0; slot < RenderTargetSet::MAX_COLOR_ATTACHMENTS; slot++)
        {
            render_targets_.set_color_attachment(slot, color_textures[slot]);
        }
        render_targets_.set_depth_stencil_attachment(info.depth_attachment ? info.depth_attachment->texture : nullptr);
        render_targets_.apply_draw_buffers();
#ifndef NDEBUG
        if (!render_targets_.is_complete())
        {
            log::log(log::LogLevel::eCritical, "Render graph: framebuffer is not complete for pass: " + std::string(info.name) + "\n");
        }
#endif
    }

    rdx_.set_viewport({ 0, 0, info.width, info.height });
    // every pass starts from the same known state, so passes don't depend on each other
    rdx_.reset_pipeline_state();

    for (const auto& attachment : info.color_attachments)
    {
        if (attachment.load_op != rg::LoadOp::eClear)
        {
            continue;
        }
        if (attachment.desc.layout == DataLayout::eR_U32)
        {
            RenderTargetSet::clear_color_uint(attachment.slot, attachment.clear.uint_value);
        }
        else
        {
            RenderTargetSet::clear_color_float(attachment.slot, attachment.clear.color);
        }
    }
    if (info.depth_attachment && info.depth_attachment->load_op == rg::LoadOp::eClear)
    {
        RenderTargetSet::clear_depth_stencil(info.depth_attachment->clear.depth, info.depth_attachment->clear.stencil);
    }
}

void engine::GlGraphBackend::end_pass()
{
    rdx_.end_debug_group();
}

void engine::GlGraphBackend::memory_barrier()
{
    // ToDo: use narrower barrier bits, based on how the resources were used
    rdx_.dispatch_barrier(RenderContext::MEMORY_BARRIER_ALL_BIT);
}

void engine::GlGraphBackend::new_frame()
{
    frame_index_++;
    std::erase_if(pool_, [this](const PoolEntry& entry)
        {
            return entry.texture.use_count() == 1 && (frame_index_ - entry.last_used_frame) > POOL_UNUSED_FRAMES_BEFORE_RELEASE;
        });
}
