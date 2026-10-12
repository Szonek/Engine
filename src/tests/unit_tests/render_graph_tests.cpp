#include "render/render_graph.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// The render graph logic doesn't depend on a graphics API. Tests provide their own texture type and backend.
namespace engine
{
class Texture2D
{
public:
    explicit Texture2D(int id_) : id(id_) {}
    int id;
};
}  // namespace engine

namespace
{
using namespace engine;

class MockBackend final : public rg::GraphBackend
{
public:
    std::shared_ptr<Texture2D> acquire_texture(const rg::TextureDesc& desc, std::string_view name) override
    {
        events.push_back("acquire " + std::string(name));
        for (auto& entry : pool)
        {
            if (entry.second.use_count() == 1 && entry.first == desc)
            {
                return entry.second;
            }
        }
        auto texture = std::make_shared<Texture2D>(next_id++);
        pool.emplace_back(desc, texture);
        return texture;
    }

    void begin_pass(const rg::PassBeginInfo& info) override
    {
        events.push_back("begin " + std::string(info.name));
        last_begin_info_name = std::string(info.name);
        begin_infos.push_back({ std::string(info.name), info.is_raster, info.width, info.height, {}, info.depth_attachment != nullptr });
        for (const auto& attachment : info.color_attachments)
        {
            begin_infos.back().color_slots.push_back({ attachment.slot, attachment.load_op });
        }
    }
    void end_pass() override { events.push_back("end"); }
    void memory_barrier() override { events.push_back("barrier"); }

    struct BeginInfo
    {
        std::string name;
        bool is_raster;
        std::uint32_t width;
        std::uint32_t height;
        std::vector<std::pair<std::uint32_t, rg::LoadOp>> color_slots;
        bool has_depth;
    };

    std::vector<std::string> events;
    std::vector<BeginInfo> begin_infos;
    std::string last_begin_info_name;
    std::vector<std::pair<rg::TextureDesc, std::shared_ptr<Texture2D>>> pool;
    int next_id = 0;
};

constexpr rg::TextureDesc DESC{ 64, 64, DataLayout::eRGBA_U8 };

// passes which only declare resource usage and record that they were executed
struct Recorder
{
    std::vector<std::string> executed;
};

template<typename Setup>
void add_pass(rg::Graph& graph, Recorder& recorder, const std::string& name, Setup&& setup)
{
    graph.add_pass(name,
        [&setup](rg::Builder& builder, rg::NoPassData&) { setup(builder); },
        [&recorder, name](const rg::NoPassData&, rg::PassContext&) { recorder.executed.push_back(name); });
}

bool contains(const std::vector<std::string>& v, const std::string& s)
{
    return std::ranges::find(v, s) != v.end();
}

std::size_t index_of(const std::vector<std::string>& v, const std::string& s)
{
    return static_cast<std::size_t>(std::ranges::find(v, s) - v.begin());
}
}  // namespace

TEST(RenderGraph, PassWithoutConsumersIsCulled)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);

    add_pass(graph, recorder, "unused", [&](rg::Builder& b) { b.write_color(0, b.create_texture("unused_tex", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.execute(backend);

    EXPECT_EQ(recorder.executed, std::vector<std::string>{ "present" });
    const auto infos = graph.get_pass_infos();
    EXPECT_TRUE(infos[0].culled);
    EXPECT_FALSE(infos[1].culled);
    // culled pass doesn't allocate anything
    EXPECT_FALSE(contains(backend.events, "acquire unused_tex"));
}

TEST(RenderGraph, ProducerOfConsumedTextureIsKept)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle tex;

    add_pass(graph, recorder, "producer", [&](rg::Builder& b) { tex = b.write_color(0, b.create_texture("tex", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "consumer", [&](rg::Builder& b) { b.read(tex); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.execute(backend);

    EXPECT_EQ(recorder.executed, (std::vector<std::string>{ "producer", "consumer" }));
}

TEST(RenderGraph, SideEffectPassIsNeverCulled)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;

    add_pass(graph, recorder, "side_effect", [&](rg::Builder& b) { b.write_color(0, b.create_texture("tex", DESC), rg::LoadOp::eClear); b.set_side_effect(); });

    graph.execute(backend);

    EXPECT_EQ(recorder.executed, std::vector<std::string>{ "side_effect" });
}

TEST(RenderGraph, ClearMakesEarlierWritersUnnecessary)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle tex;

    add_pass(graph, recorder, "first_write", [&](rg::Builder& b) { tex = b.write_color(0, b.create_texture("tex", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "overwrite", [&](rg::Builder& b) { b.write_color(0, tex, rg::LoadOp::eClear); });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.read(tex); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.execute(backend);

    EXPECT_EQ(recorder.executed, (std::vector<std::string>{ "overwrite", "present" }));
}

TEST(RenderGraph, LoadKeepsEarlierWriters)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle tex;

    add_pass(graph, recorder, "clear", [&](rg::Builder& b) { tex = b.write_color(0, b.create_texture("tex", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "draw_a", [&](rg::Builder& b) { b.write_color(0, tex, rg::LoadOp::eLoad); });
    add_pass(graph, recorder, "draw_b", [&](rg::Builder& b) { b.write_color(0, tex, rg::LoadOp::eLoad); });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.read(tex); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.execute(backend);

    EXPECT_EQ(recorder.executed, (std::vector<std::string>{ "clear", "draw_a", "draw_b", "present" }));
}

TEST(RenderGraph, PassesWritingUnneededSecondaryOutputsAreCulled)
{
    // "mask" is produced and then consumed only by a culled pass -> both are culled
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle mask;
    rg::TextureHandle blurred;

    add_pass(graph, recorder, "mask", [&](rg::Builder& b) { mask = b.write_color(0, b.create_texture("mask", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "blur", [&](rg::Builder& b) { b.read(mask); blurred = b.write_color(0, b.create_texture("blurred", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.execute(backend);

    EXPECT_EQ(recorder.executed, std::vector<std::string>{ "present" });
}

TEST(RenderGraph, ExecutesPassesInDeclarationOrder)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);

    for (int i = 0; i < 5; i++)
    {
        add_pass(graph, recorder, "pass" + std::to_string(i), [&](rg::Builder& b) { b.write_color(0, backbuffer, rg::LoadOp::eLoad); });
    }
    graph.execute(backend);

    EXPECT_EQ(recorder.executed, (std::vector<std::string>{ "pass0", "pass1", "pass2", "pass3", "pass4" }));
    EXPECT_EQ(graph.get_execution_order(), recorder.executed);
}

TEST(RenderGraph, TransientTexturesWithDisjointLifetimesShareMemory)
{
    rg::Graph graph;
    MockBackend backend;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle a, b_tex, c;
    int id_a = -1, id_b = -1, id_c = -1;
    const auto noop = [](const rg::NoPassData&, rg::PassContext&) {};

    graph.add_pass("p0", [&](rg::Builder& b, rg::NoPassData&) { a = b.write_color(0, b.create_texture("A", DESC), rg::LoadOp::eClear); },
        [&](const rg::NoPassData&, rg::PassContext& ctx) { id_a = ctx.texture(a).id; });
    graph.add_pass("p1", [&](rg::Builder& b, rg::NoPassData&) { b.read(a); b_tex = b.write_color(0, b.create_texture("B", DESC), rg::LoadOp::eClear); },
        [&](const rg::NoPassData&, rg::PassContext& ctx) { id_b = ctx.texture(b_tex).id; });
    graph.add_pass("p2", [&](rg::Builder& b, rg::NoPassData&) { b.read(b_tex); c = b.write_color(0, b.create_texture("C", DESC), rg::LoadOp::eClear); },
        [&](const rg::NoPassData&, rg::PassContext& ctx) { id_c = ctx.texture(c).id; });
    graph.add_pass("p3", [&](rg::Builder& b, rg::NoPassData&) { b.read(c); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); }, noop);

    graph.execute(backend);

    EXPECT_NE(id_a, id_b);  // A and B are alive at the same time (p1)
    EXPECT_NE(id_b, id_c);  // B and C are alive at the same time (p2)
    EXPECT_EQ(id_a, id_c);  // A is dead after p1, so C reuses it
    EXPECT_EQ(backend.pool.size(), 2u);
}

TEST(RenderGraph, TexturesWithDifferentDescriptionsAreNotShared)
{
    rg::Graph graph;
    MockBackend backend;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle a, c;
    const auto other_desc = rg::TextureDesc{ 32, 32, DataLayout::eRGBA_U8 };
    const auto noop = [](const rg::NoPassData&, rg::PassContext&) {};

    graph.add_pass("p0", [&](rg::Builder& b, rg::NoPassData&) { a = b.write_color(0, b.create_texture("A", DESC), rg::LoadOp::eClear); }, noop);
    graph.add_pass("p1", [&](rg::Builder& b, rg::NoPassData&) { b.read(a); c = b.write_color(0, b.create_texture("C", other_desc), rg::LoadOp::eClear); }, noop);
    graph.add_pass("p2", [&](rg::Builder& b, rg::NoPassData&) { b.read(c); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); }, noop);

    graph.execute(backend);

    EXPECT_EQ(backend.pool.size(), 2u);
}

TEST(RenderGraph, ExtractedTextureOutlivesTheGraph)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    std::shared_ptr<Texture2D> extracted;
    rg::TextureHandle tex;

    add_pass(graph, recorder, "producer", [&](rg::Builder& b) { tex = b.write_color(0, b.create_texture("tex", DESC), rg::LoadOp::eClear); });
    graph.extract_texture(tex, &extracted);

    graph.execute(backend);

    // extraction is an output of the graph, so the producer is not culled
    EXPECT_EQ(recorder.executed, std::vector<std::string>{ "producer" });
    ASSERT_NE(extracted, nullptr);
    // pool + extracted, graph has released its reference
    EXPECT_EQ(extracted.use_count(), 2);

    // the texture is not handed out again while somebody holds it
    graph.reset();
    rg::TextureHandle tex2;
    add_pass(graph, recorder, "producer2", [&](rg::Builder& b) { tex2 = b.write_color(0, b.create_texture("tex2", DESC), rg::LoadOp::eClear); b.set_side_effect(); });
    graph.execute(backend);
    EXPECT_EQ(backend.pool.size(), 2u);
}

TEST(RenderGraph, RasterPassReceivesAttachments)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 128, 64);
    const auto desc = rg::TextureDesc{ 128, 64, DataLayout::eRGBA_U8 };
    rg::TextureHandle color, ids, depth;

    add_pass(graph, recorder, "clear", [&](rg::Builder& b)
        {
            color = b.write_color(0, b.create_texture("color", desc), rg::LoadOp::eClear);
            ids = b.write_color(1, b.create_texture("ids", { 128, 64, DataLayout::eR_U32 }), rg::LoadOp::eClear);
            depth = b.write_depth(b.create_texture("depth", { 128, 64, DataLayout::eDEPTH24_STENCIL8_U32 }), rg::LoadOp::eClear);
        });
    add_pass(graph, recorder, "draw", [&](rg::Builder& b)
        {
            b.write_color(0, color, rg::LoadOp::eLoad);
            b.write_depth(depth, rg::LoadOp::eLoad);
        });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.read(color); b.read(ids); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.execute(backend);

    ASSERT_EQ(backend.begin_infos.size(), 3u);
    const auto& clear = backend.begin_infos[0];
    EXPECT_TRUE(clear.is_raster);
    EXPECT_EQ(clear.width, 128u);
    EXPECT_EQ(clear.height, 64u);
    EXPECT_TRUE(clear.has_depth);
    ASSERT_EQ(clear.color_slots.size(), 2u);
    EXPECT_EQ(clear.color_slots[0], (std::pair<std::uint32_t, rg::LoadOp>{ 0, rg::LoadOp::eClear }));
    EXPECT_EQ(clear.color_slots[1], (std::pair<std::uint32_t, rg::LoadOp>{ 1, rg::LoadOp::eClear }));

    const auto& draw = backend.begin_infos[1];
    ASSERT_EQ(draw.color_slots.size(), 1u);
    EXPECT_EQ(draw.color_slots[0].second, rg::LoadOp::eLoad);
    EXPECT_TRUE(draw.has_depth);
}

TEST(RenderGraph, PassWithoutAttachmentsIsNotRaster)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    rg::TextureHandle tex;

    add_pass(graph, recorder, "compute", [&](rg::Builder& b) { tex = b.write_image(b.create_texture("tex", DESC), false); b.set_side_effect(); });

    graph.execute(backend);

    ASSERT_EQ(backend.begin_infos.size(), 1u);
    EXPECT_FALSE(backend.begin_infos[0].is_raster);
}

TEST(RenderGraph, ImagePassesGetMemoryBarriers)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle mask, color;

    add_pass(graph, recorder, "mask", [&](rg::Builder& b) { mask = b.write_color(0, b.create_texture("mask", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "clear_color", [&](rg::Builder& b) { color = b.write_color(0, b.create_texture("color", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "edges", [&](rg::Builder& b) { b.read_image(mask); b.write_image(color); });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.read(color); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.execute(backend);

    // barrier before the image pass (it has to see the rendered mask) and before present (it has to see image stores)
    const auto& e = backend.events;
    const auto edges_begin = index_of(e, "begin edges");
    const auto present_begin = index_of(e, "begin present");
    ASSERT_GT(edges_begin, 0u);
    EXPECT_EQ(e[edges_begin - 1], "barrier");
    ASSERT_GT(present_begin, 0u);
    EXPECT_EQ(e[present_begin - 1], "barrier");
    EXPECT_EQ(std::ranges::count(e, "barrier"), 2);
}

TEST(RenderGraph, NoBarriersWithoutImageAccess)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle tex;

    add_pass(graph, recorder, "draw", [&](rg::Builder& b) { tex = b.write_color(0, b.create_texture("tex", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.read(tex); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.execute(backend);

    EXPECT_FALSE(contains(backend.events, "barrier"));
}

TEST(RenderGraph, ReadingTextureNobodyWroteIsReported)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle tex;

    add_pass(graph, recorder, "create_only", [&](rg::Builder& b) { tex = b.create_texture("never_written", DESC); });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.read(tex); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.compile();

    ASSERT_EQ(graph.get_messages().size(), 1u);
    EXPECT_NE(graph.get_messages()[0].find("never_written"), std::string::npos);
}

TEST(RenderGraph, AttachmentsOfDifferentSizeInvalidatePass)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);

    add_pass(graph, recorder, "broken", [&](rg::Builder& b)
        {
            b.write_color(0, b.create_texture("small", { 32, 32, DataLayout::eRGBA_U8 }), rg::LoadOp::eClear);
            b.write_color(1, b.create_texture("big", { 64, 64, DataLayout::eRGBA_U8 }), rg::LoadOp::eClear);
            b.set_side_effect();
        });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });

    graph.execute(backend);

    EXPECT_FALSE(graph.get_messages().empty());
    EXPECT_EQ(recorder.executed, std::vector<std::string>{ "present" });
}

TEST(RenderGraph, BackbufferCantBeCombinedWithOtherAttachments)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);

    add_pass(graph, recorder, "broken", [&](rg::Builder& b)
        {
            b.write_color(0, backbuffer, rg::LoadOp::eLoad);
            b.write_color(1, b.create_texture("other", DESC), rg::LoadOp::eClear);
        });

    graph.execute(backend);

    EXPECT_FALSE(graph.get_messages().empty());
    EXPECT_TRUE(recorder.executed.empty());
}

TEST(RenderGraph, TextureCantBeAttachmentAndShaderResourceInOnePass)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    rg::TextureHandle tex;

    add_pass(graph, recorder, "init", [&](rg::Builder& b) { tex = b.write_color(0, b.create_texture("tex", DESC), rg::LoadOp::eClear); b.set_side_effect(); });
    add_pass(graph, recorder, "feedback", [&](rg::Builder& b) { b.read(tex); b.write_color(0, tex, rg::LoadOp::eLoad); b.set_side_effect(); });

    graph.execute(backend);

    EXPECT_FALSE(graph.get_messages().empty());
    EXPECT_EQ(recorder.executed, std::vector<std::string>{ "init" });
}

TEST(RenderGraph, ResetAllowsBuildingTheNextFrame)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;

    for (int frame = 0; frame < 3; frame++)
    {
        graph.reset();
        const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
        rg::TextureHandle tex;
        add_pass(graph, recorder, "draw", [&](rg::Builder& b) { tex = b.write_color(0, b.create_texture("tex", DESC), rg::LoadOp::eClear); });
        add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.read(tex); b.write_color(0, backbuffer, rg::LoadOp::eDontCare); });
        graph.execute(backend);
    }

    EXPECT_EQ(recorder.executed.size(), 6u);
    // the same physical texture is recycled every frame
    EXPECT_EQ(backend.pool.size(), 1u);
}

TEST(RenderGraph, ResourceInfoContainsLifetimes)
{
    rg::Graph graph;
    MockBackend backend;
    Recorder recorder;
    const auto backbuffer = graph.import_backbuffer("backbuffer", 64, 64);
    rg::TextureHandle tex;

    add_pass(graph, recorder, "draw", [&](rg::Builder& b) { tex = b.write_color(0, b.create_texture("tex", DESC), rg::LoadOp::eClear); });
    add_pass(graph, recorder, "other", [&](rg::Builder& b) { b.write_color(0, backbuffer, rg::LoadOp::eLoad); });
    add_pass(graph, recorder, "present", [&](rg::Builder& b) { b.read(tex); b.write_color(0, backbuffer, rg::LoadOp::eLoad); });

    graph.compile();

    const auto resources = graph.get_resource_infos();
    ASSERT_EQ(resources.size(), 2u);
    EXPECT_TRUE(resources[0].imported);
    EXPECT_EQ(resources[1].name, "tex");
    EXPECT_EQ(resources[1].first_pass, 0u);
    EXPECT_EQ(resources[1].last_pass, 2u);
}
