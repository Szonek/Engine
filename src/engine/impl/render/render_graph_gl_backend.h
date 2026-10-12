#pragma once
#include "render_graph.h"
#include "../graphics.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace engine
{
// OpenGL implementation of the render graph backend:
//  - owns a pool of render target textures (transient textures are recycled between passes and frames),
//  - binds framebuffer + viewport, resets pipeline state and applies clear ops for raster passes,
//  - wraps every pass in a debug group (visible in RenderDoc / Nsight),
//  - issues memory barriers.
class GlGraphBackend final : public rg::GraphBackend
{
public:
    explicit GlGraphBackend(RenderContext& rdx);

    std::shared_ptr<Texture2D> acquire_texture(const rg::TextureDesc& desc, std::string_view name) override;
    void begin_pass(const rg::PassBeginInfo& info) override;
    void end_pass() override;
    void memory_barrier() override;

    // Call once per frame. Releases textures which were not used for a few frames (i.e. after window resize).
    void new_frame();

    std::size_t get_pooled_texture_count() const { return pool_.size(); }

private:
    struct PoolEntry
    {
        rg::TextureDesc desc;
        std::shared_ptr<Texture2D> texture;
        std::uint64_t last_used_frame = 0;
    };

    RenderContext& rdx_;
    RenderTargetSet render_targets_;
    std::vector<PoolEntry> pool_;
    std::uint64_t frame_index_ = 0;
};

}  // namespace engine
