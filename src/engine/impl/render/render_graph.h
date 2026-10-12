#pragma once
#include "../data_layout.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace engine
{
class Texture2D;
}

// Render graph (a.k.a. frame graph).
//
// Every frame the renderer *declares* passes. For each pass a setup callback runs immediately and tells the graph which textures
// the pass creates / reads / writes. The execute callback is stored and runs later, once the whole frame is known.
// Knowing the whole frame lets the graph:
//   - cull passes which do not contribute to any output (backbuffer, extracted textures, passes with side effects),
//   - allocate transient textures lazily and alias them (a texture is returned to the pool after its last use),
//   - bind framebuffers, viewport, apply clear/load ops and insert memory barriers for passes, so passes only issue draw calls.
//
// This file is intentionally free of any graphics API calls. All of that is delegated to GraphBackend (see render_graph_gl_backend.h),
// so the graph logic can be unit tested without a GL context.
namespace engine::rg
{
inline constexpr std::uint32_t INVALID_INDEX = 0xFFFFFFFFu;

struct TextureHandle
{
    std::uint32_t index = INVALID_INDEX;

    bool is_valid() const { return index != INVALID_INDEX; }
    friend bool operator==(const TextureHandle&, const TextureHandle&) = default;
};

struct TextureDesc
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    DataLayout layout = DataLayout::eRGBA_U8;

    friend bool operator==(const TextureDesc&, const TextureDesc&) = default;
};

enum class LoadOp : std::uint8_t
{
    eLoad,      // keep existing content
    eClear,     // clear to ClearValue
    eDontCare   // content is fully overwritten by the pass
};

struct ClearValue
{
    std::array<float, 4> color = { 0.0f, 0.0f, 0.0f, 1.0f };
    std::uint32_t uint_value = 0;   // used for integer color formats
    float depth = 1.0f;
    std::uint8_t stencil = 0;
};

enum class UseType : std::uint8_t
{
    eSampled,         // sampled in a shader (sampler2D)
    eImageRead,       // imageLoad
    eImageWrite,      // imageStore (previous content is discarded)
    eImageReadWrite,  // imageLoad + imageStore
    eColorAttachment,
    eDepthAttachment
};

// One way a pass touches a texture.
struct ResourceUse
{
    TextureHandle texture;
    UseType type = UseType::eSampled;
    std::uint32_t slot = 0;  // color attachment slot
    LoadOp load_op = LoadOp::eLoad;
    ClearValue clear;

    bool is_attachment() const { return type == UseType::eColorAttachment || type == UseType::eDepthAttachment; }
    bool is_image() const { return type == UseType::eImageRead || type == UseType::eImageWrite || type == UseType::eImageReadWrite; }
    // does the pass depend on the content which was in the texture before the pass?
    bool reads_previous_content() const
    {
        switch (type)
        {
        case UseType::eSampled:
        case UseType::eImageRead:
        case UseType::eImageReadWrite:
            return true;
        case UseType::eColorAttachment:
        case UseType::eDepthAttachment:
            return load_op == LoadOp::eLoad;
        default:
            return false;
        }
    }
    bool writes() const { return type != UseType::eSampled && type != UseType::eImageRead; }
};

// What a backend receives for each attachment of a raster pass.
struct ResolvedAttachment
{
    std::uint32_t slot = 0;
    Texture2D* texture = nullptr;  // nullptr for the backbuffer
    bool is_backbuffer = false;
    TextureDesc desc;
    LoadOp load_op = LoadOp::eLoad;
    ClearValue clear;
};

struct PassBeginInfo
{
    std::string_view name;
    bool is_raster = false;  // has attachments, framebuffer is bound and cleared by the backend
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::span<const ResolvedAttachment> color_attachments;
    const ResolvedAttachment* depth_attachment = nullptr;
};

// Everything API specific the graph needs.
class GraphBackend
{
public:
    virtual ~GraphBackend() = default;

    virtual std::shared_ptr<Texture2D> acquire_texture(const TextureDesc& desc, std::string_view name) = 0;
    virtual void begin_pass(const PassBeginInfo& info) = 0;
    virtual void end_pass() = 0;
    // makes writes done by image stores / rendering visible to the following passes
    virtual void memory_barrier() = 0;
};

class Graph;

// Used by pass execute callbacks to get physical textures.
class PassContext
{
public:
    Texture2D& texture(TextureHandle handle) const;
    const TextureDesc& desc(TextureHandle handle) const;
    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }

private:
    friend class Graph;
    PassContext(const Graph& graph, std::uint32_t width, std::uint32_t height) : graph_(graph), width_(width), height_(height) {}

    const Graph& graph_;
    std::uint32_t width_;
    std::uint32_t height_;
};

struct PassNode
{
    std::string name;
    std::vector<ResourceUse> uses;
    bool side_effect = false;
    bool culled = false;
    bool invalid = false;
};

// Used by pass setup callbacks to declare what a pass does.
class Builder
{
public:
    // Transient texture, lives only as long as the passes using it. Content is undefined until the first write.
    TextureHandle create_texture(std::string_view name, const TextureDesc& desc);

    TextureHandle read(TextureHandle texture);        // sampled
    TextureHandle read_image(TextureHandle texture);  // imageLoad
    // imageStore. If preserve_content is false previous content is discarded (not a dependency on previous writers).
    TextureHandle write_image(TextureHandle texture, bool preserve_content = true);

    // Raster outputs. A pass with at least one attachment is a raster pass: the framebuffer is bound by the graph.
    TextureHandle write_color(std::uint32_t slot, TextureHandle texture, LoadOp load_op, const ClearValue& clear = {});
    TextureHandle write_depth(TextureHandle texture, LoadOp load_op, const ClearValue& clear = {});

    // Pass is never culled (i.e. it draws UI directly to the screen).
    void set_side_effect();

    const TextureDesc& desc(TextureHandle texture) const;

private:
    friend class Graph;
    Builder(Graph& graph, PassNode& node) : graph_(graph), node_(node) {}

    TextureHandle add_use(TextureHandle texture, UseType type, std::uint32_t slot = 0, LoadOp load_op = LoadOp::eLoad, const ClearValue& clear = {});

    Graph& graph_;
    PassNode& node_;
};

struct NoPassData
{
};

struct PassInfo
{
    std::string name;
    bool culled = false;
    std::vector<std::string> reads;
    std::vector<std::string> writes;
};

struct ResourceInfo
{
    std::string name;
    TextureDesc desc;
    bool imported = false;
    bool extracted = false;
    std::uint32_t first_pass = INVALID_INDEX;  // index in execution order
    std::uint32_t last_pass = INVALID_INDEX;
};

class Graph
{
public:
    Graph() = default;
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;
    ~Graph() = default;

    // Drops all passes and resources, call at the beginning of every frame.
    void reset();

    // Declares a pass.
    //   setup:   void(Builder&, PassData&)             - runs immediately, declares resource usage, stores handles in PassData
    //   execute: void(const PassData&, PassContext&)   - runs in Graph::execute, only if the pass was not culled
    // Returns PassData so following passes can use handles created by this pass.
    template<typename PassData = NoPassData, typename SetupFn, typename ExecuteFn>
    PassData& add_pass(std::string_view name, SetupFn&& setup, ExecuteFn&& execute)
    {
        auto pass = std::make_unique<TypedPass<PassData, std::decay_t<ExecuteFn>>>(std::forward<ExecuteFn>(execute));
        pass->node.name = name;
        PassData& data = pass->data;
        Builder builder(*this, pass->node);
        setup(builder, data);
        passes_.push_back(std::move(pass));
        compiled_ = false;
        return data;
    }

    // The screen. Always considered as an output of the graph.
    TextureHandle import_backbuffer(std::string_view name, std::uint32_t width, std::uint32_t height);
    // Texture owned by someone else. Always considered as an output of the graph.
    TextureHandle import_texture(std::string_view name, std::shared_ptr<Texture2D> texture, const TextureDesc& desc);

    // Keeps the physical texture alive after the graph is executed. *out is assigned during execute.
    // Useful for textures needed next frame (i.e. reading entity ids for picking).
    void extract_texture(TextureHandle texture, std::shared_ptr<Texture2D>* out);

    // Culls passes and computes lifetimes. Called by execute() if needed.
    void compile();
    void execute(GraphBackend& backend);

    // Problems found while declaring/compiling the graph (i.e. reading a texture nobody wrote).
    const std::vector<std::string>& get_messages() const { return messages_; }
    std::vector<PassInfo> get_pass_infos() const;
    std::vector<ResourceInfo> get_resource_infos() const;
    // Names of passes which are executed, in the execution order.
    std::vector<std::string> get_execution_order() const;

private:
    friend class Builder;
    friend class PassContext;

    struct PassBase
    {
        virtual ~PassBase() = default;
        virtual void execute(PassContext& ctx) = 0;
        PassNode node;
    };

    template<typename PassData, typename ExecuteFn>
    struct TypedPass final : PassBase
    {
        explicit TypedPass(ExecuteFn fn) : execute_fn(std::move(fn)) {}
        void execute(PassContext& ctx) override { execute_fn(std::as_const(data), ctx); }

        PassData data{};
        ExecuteFn execute_fn;
    };

    struct Resource
    {
        std::string name;
        TextureDesc desc;
        bool imported = false;
        bool is_backbuffer = false;
        std::shared_ptr<Texture2D> physical;
        std::shared_ptr<Texture2D>* extract_target = nullptr;

        std::uint32_t first_use = INVALID_INDEX;  // position in execution order
        std::uint32_t last_use = INVALID_INDEX;
    };

    TextureHandle create_resource(std::string_view name, const TextureDesc& desc, bool imported);
    bool is_valid_handle(TextureHandle handle) const { return handle.index < resources_.size(); }
    void validate();
    void cull_and_order();
    void compute_lifetimes();
    void release_resource(Resource& resource);

    std::vector<std::unique_ptr<PassBase>> passes_;
    std::vector<Resource> resources_;
    std::vector<std::uint32_t> execution_order_;  // indices into passes_
    std::vector<std::string> messages_;
    bool compiled_ = false;
};

}  // namespace engine::rg
