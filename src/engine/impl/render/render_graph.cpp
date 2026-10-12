#include "render_graph.h"

#include <algorithm>
#include <cassert>
#include <string>

namespace engine::rg
{
namespace
{
constexpr std::uint32_t MAX_COLOR_ATTACHMENTS = 8;
}

Texture2D& PassContext::texture(TextureHandle handle) const
{
    assert(graph_.is_valid_handle(handle));
    const auto& resource = graph_.resources_[handle.index];
    assert(resource.physical && "Texture is not available. Backbuffer or texture which is not used by this pass?");
    return *resource.physical;
}

const TextureDesc& PassContext::desc(TextureHandle handle) const
{
    assert(graph_.is_valid_handle(handle));
    return graph_.resources_[handle.index].desc;
}

TextureHandle Builder::create_texture(std::string_view name, const TextureDesc& desc)
{
    return graph_.create_resource(name, desc, false);
}

TextureHandle Builder::read(TextureHandle texture)
{
    return add_use(texture, UseType::eSampled);
}

TextureHandle Builder::read_image(TextureHandle texture)
{
    return add_use(texture, UseType::eImageRead);
}

TextureHandle Builder::write_image(TextureHandle texture, bool preserve_content)
{
    return add_use(texture, preserve_content ? UseType::eImageReadWrite : UseType::eImageWrite);
}

TextureHandle Builder::write_color(std::uint32_t slot, TextureHandle texture, LoadOp load_op, const ClearValue& clear)
{
    return add_use(texture, UseType::eColorAttachment, slot, load_op, clear);
}

TextureHandle Builder::write_depth(TextureHandle texture, LoadOp load_op, const ClearValue& clear)
{
    return add_use(texture, UseType::eDepthAttachment, 0, load_op, clear);
}

void Builder::set_side_effect()
{
    node_.side_effect = true;
}

const TextureDesc& Builder::desc(TextureHandle texture) const
{
    assert(graph_.is_valid_handle(texture));
    return graph_.resources_[texture.index].desc;
}

TextureHandle Builder::add_use(TextureHandle texture, UseType type, std::uint32_t slot, LoadOp load_op, const ClearValue& clear)
{
    assert(graph_.is_valid_handle(texture) && "Invalid texture handle used in the render graph pass");
    node_.uses.push_back(ResourceUse{ .texture = texture, .type = type, .slot = slot, .load_op = load_op, .clear = clear });
    return texture;
}

void Graph::reset()
{
    passes_.clear();
    resources_.clear();
    execution_order_.clear();
    messages_.clear();
    compiled_ = false;
}

TextureHandle Graph::create_resource(std::string_view name, const TextureDesc& desc, bool imported)
{
    Resource resource;
    resource.name = name;
    resource.desc = desc;
    resource.imported = imported;
    resources_.push_back(std::move(resource));
    compiled_ = false;
    return TextureHandle{ static_cast<std::uint32_t>(resources_.size() - 1) };
}

TextureHandle Graph::import_backbuffer(std::string_view name, std::uint32_t width, std::uint32_t height)
{
    const auto handle = create_resource(name, TextureDesc{ width, height, DataLayout::eRGBA_U8 }, true);
    resources_[handle.index].is_backbuffer = true;
    return handle;
}

TextureHandle Graph::import_texture(std::string_view name, std::shared_ptr<Texture2D> texture, const TextureDesc& desc)
{
    const auto handle = create_resource(name, desc, true);
    resources_[handle.index].physical = std::move(texture);
    return handle;
}

void Graph::extract_texture(TextureHandle texture, std::shared_ptr<Texture2D>* out)
{
    assert(is_valid_handle(texture));
    assert(out != nullptr);
    resources_[texture.index].extract_target = out;
    compiled_ = false;
}

void Graph::validate()
{
    std::vector<bool> written(resources_.size(), false);
    for (std::size_t i = 0; i < resources_.size(); i++)
    {
        written[i] = resources_[i].imported;
    }

    for (const auto& pass_ptr : passes_)
    {
        auto& node = pass_ptr->node;
        auto fail = [&](const std::string& what)
        {
            messages_.push_back("Pass '" + node.name + "': " + what);
            node.invalid = true;
        };

        std::uint32_t used_slots = 0;
        std::uint32_t depth_count = 0;
        std::uint32_t attachments_count = 0;
        const TextureDesc* attachment_desc = nullptr;
        bool has_backbuffer_attachment = false;

        for (const auto& use : node.uses)
        {
            const auto& resource = resources_[use.texture.index];
            if (use.reads_previous_content() && !written[use.texture.index])
            {
                messages_.push_back("Pass '" + node.name + "' reads texture '" + resource.name + "' which was not written by any earlier pass");
            }

            if (use.is_attachment())
            {
                attachments_count++;
                has_backbuffer_attachment |= resource.is_backbuffer;
                if (use.type == UseType::eDepthAttachment)
                {
                    depth_count++;
                }
                else
                {
                    if (use.slot >= MAX_COLOR_ATTACHMENTS)
                    {
                        fail("color attachment slot " + std::to_string(use.slot) + " is out of range");
                    }
                    else if (used_slots & (1u << use.slot))
                    {
                        fail("color attachment slot " + std::to_string(use.slot) + " is used twice");
                    }
                    else
                    {
                        used_slots |= (1u << use.slot);
                    }
                }
                if (attachment_desc && (attachment_desc->width != resource.desc.width || attachment_desc->height != resource.desc.height))
                {
                    fail("attachment '" + resource.name + "' has different size than other attachments");
                }
                attachment_desc = attachment_desc ? attachment_desc : &resource.desc;
            }
        }

        if (depth_count > 1)
        {
            fail("more than one depth attachment");
        }
        if (has_backbuffer_attachment && attachments_count != 1)
        {
            fail("backbuffer can't be combined with other attachments");
        }
        // texture can't be sampled/imaged and rendered to by the same pass
        for (const auto& use : node.uses)
        {
            if (use.is_attachment())
            {
                continue;
            }
            const bool also_attachment = std::ranges::any_of(node.uses, [&](const ResourceUse& other) { return other.is_attachment() && other.texture == use.texture; });
            if (also_attachment)
            {
                fail("texture '" + resources_[use.texture.index].name + "' is used as an attachment and as a shader resource at the same time");
                break;
            }
        }

        for (const auto& use : node.uses)
        {
            if (use.writes())
            {
                written[use.texture.index] = true;
            }
        }
    }
}

void Graph::cull_and_order()
{
    std::vector<bool> needed(resources_.size(), false);
    for (std::size_t i = 0; i < resources_.size(); i++)
    {
        needed[i] = resources_[i].imported || resources_[i].extract_target != nullptr;
    }

    // Walk backwards: a pass is alive if it has a side effect or writes something which somebody (alive) needs later.
    for (auto it = passes_.rbegin(); it != passes_.rend(); ++it)
    {
        auto& node = (*it)->node;
        if (node.invalid)
        {
            node.culled = true;
            continue;
        }
        bool alive = node.side_effect;
        for (const auto& use : node.uses)
        {
            alive |= use.writes() && needed[use.texture.index];
        }
        node.culled = !alive;
        if (!alive)
        {
            continue;
        }
        // Full overwrite: whatever was in the texture before this pass is not needed anymore.
        for (const auto& use : node.uses)
        {
            if (use.writes() && !use.reads_previous_content())
            {
                needed[use.texture.index] = false;
            }
        }
        for (const auto& use : node.uses)
        {
            if (use.reads_previous_content())
            {
                needed[use.texture.index] = true;
            }
        }
    }

    execution_order_.clear();
    for (std::uint32_t i = 0; i < passes_.size(); i++)
    {
        if (!passes_[i]->node.culled)
        {
            execution_order_.push_back(i);
        }
    }
}

void Graph::compute_lifetimes()
{
    for (auto& resource : resources_)
    {
        resource.first_use = INVALID_INDEX;
        resource.last_use = INVALID_INDEX;
    }
    for (std::uint32_t position = 0; position < execution_order_.size(); position++)
    {
        for (const auto& use : passes_[execution_order_[position]]->node.uses)
        {
            auto& resource = resources_[use.texture.index];
            if (resource.first_use == INVALID_INDEX)
            {
                resource.first_use = position;
            }
            resource.last_use = position;
        }
    }
}

void Graph::compile()
{
    messages_.clear();
    for (auto& pass : passes_)
    {
        pass->node.culled = false;
        pass->node.invalid = false;
    }
    validate();
    cull_and_order();
    compute_lifetimes();
    compiled_ = true;
}

void Graph::release_resource(Resource& resource)
{
    if (resource.imported)
    {
        return;
    }
    if (resource.extract_target)
    {
        *resource.extract_target = resource.physical;
    }
    resource.physical.reset();
}

void Graph::execute(GraphBackend& backend)
{
    if (!compiled_)
    {
        compile();
    }

    for (auto& resource : resources_)
    {
        if (resource.extract_target)
        {
            resource.extract_target->reset();
        }
    }

    // Resources written through images since the last barrier.
    std::vector<bool> dirty(resources_.size(), false);
    std::vector<ResolvedAttachment> color_attachments;

    for (std::uint32_t position = 0; position < execution_order_.size(); position++)
    {
        auto& pass = *passes_[execution_order_[position]];
        const auto& node = pass.node;

        bool needs_barrier = false;
        for (const auto& use : node.uses)
        {
            auto& resource = resources_[use.texture.index];
            if (!resource.imported && !resource.physical)
            {
                resource.physical = backend.acquire_texture(resource.desc, resource.name);
            }
            // Image access has to see previous rendering; anything has to see previous image stores.
            needs_barrier |= use.is_image() || dirty[use.texture.index];
        }
        if (needs_barrier)
        {
            backend.memory_barrier();
            std::fill(dirty.begin(), dirty.end(), false);
        }

        color_attachments.clear();
        ResolvedAttachment depth_attachment;
        bool has_depth = false;
        PassBeginInfo begin_info;
        begin_info.name = node.name;
        for (const auto& use : node.uses)
        {
            if (!use.is_attachment())
            {
                continue;
            }
            const auto& resource = resources_[use.texture.index];
            ResolvedAttachment attachment;
            attachment.slot = use.slot;
            attachment.texture = resource.is_backbuffer ? nullptr : resource.physical.get();
            attachment.is_backbuffer = resource.is_backbuffer;
            attachment.desc = resource.desc;
            attachment.load_op = use.load_op;
            attachment.clear = use.clear;
            begin_info.is_raster = true;
            begin_info.width = resource.desc.width;
            begin_info.height = resource.desc.height;
            if (use.type == UseType::eDepthAttachment)
            {
                depth_attachment = attachment;
                has_depth = true;
            }
            else
            {
                color_attachments.push_back(attachment);
            }
        }
        begin_info.color_attachments = color_attachments;
        begin_info.depth_attachment = has_depth ? &depth_attachment : nullptr;

        backend.begin_pass(begin_info);
        PassContext ctx(*this, begin_info.width, begin_info.height);
        pass.execute(ctx);
        backend.end_pass();

        for (const auto& use : node.uses)
        {
            if (use.is_image() && use.writes())
            {
                dirty[use.texture.index] = true;
            }
        }
        for (const auto& use : node.uses)
        {
            auto& resource = resources_[use.texture.index];
            if (resource.last_use == position)
            {
                release_resource(resource);
            }
        }
    }

    for (auto& resource : resources_)
    {
        if (resource.imported && resource.extract_target && !resource.is_backbuffer)
        {
            *resource.extract_target = resource.physical;
        }
    }
}

std::vector<PassInfo> Graph::get_pass_infos() const
{
    std::vector<PassInfo> ret;
    ret.reserve(passes_.size());
    for (const auto& pass : passes_)
    {
        PassInfo info;
        info.name = pass->node.name;
        info.culled = pass->node.culled;
        for (const auto& use : pass->node.uses)
        {
            const auto& name = resources_[use.texture.index].name;
            if (use.reads_previous_content())
            {
                info.reads.push_back(name);
            }
            if (use.writes())
            {
                info.writes.push_back(name);
            }
        }
        ret.push_back(std::move(info));
    }
    return ret;
}

std::vector<ResourceInfo> Graph::get_resource_infos() const
{
    std::vector<ResourceInfo> ret;
    ret.reserve(resources_.size());
    for (const auto& resource : resources_)
    {
        ret.push_back(ResourceInfo{
            .name = resource.name,
            .desc = resource.desc,
            .imported = resource.imported,
            .extracted = resource.extract_target != nullptr,
            .first_pass = resource.first_use,
            .last_pass = resource.last_use });
    }
    return ret;
}

std::vector<std::string> Graph::get_execution_order() const
{
    std::vector<std::string> ret;
    ret.reserve(execution_order_.size());
    for (const auto index : execution_order_)
    {
        ret.push_back(passes_[index]->node.name);
    }
    return ret;
}

}  // namespace engine::rg
