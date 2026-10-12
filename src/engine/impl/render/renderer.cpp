#include "renderer.h"
#include "../logger.h"
#include "../profiler.h"
#include "../shaders_binding_slots.h"

#include <engine.h>

#include <fmt/format.h>

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cassert>
#include <cstring>

namespace
{
constexpr std::size_t INITIAL_LIGHTS_CAPACITY = 1'000;
constexpr std::size_t INITIAL_DEBUG_LINES_CAPACITY = 8 * 1024;

// grows the buffer (never shrinks) if it can't hold required_size bytes
template<typename BufferT>
bool ensure_buffer_capacity(BufferT& buffer, std::size_t required_size)
{
    if (buffer.is_valid() && buffer.get_size() >= required_size)
    {
        return false;
    }
    buffer = BufferT(required_size);
    return true;
}
}  // namespace

engine::Renderer::Renderer(RenderContext& rdx)
    : rdx_(rdx)
    , backend_(rdx)
    , shader_debug_lines_({ "debug_physics_lines_ssbo.vs" }, { "debug_physics_lines_ssbo.fs" })
    , geometry_debug_line_(2)
    , shader_full_screen_quad_({ "full_screen_quad.vs" }, { "full_screen_quad.fs" })
    , geometry_full_screen_quad_(6)
{
}

engine::Renderer::~Renderer() = default;

void engine::Renderer::begin_frame()
{
    frame_scenes_.clear();
    material_skinned_geometry_lit_.new_frame();
}

void engine::Renderer::submit_scene(SceneRenderData&& scene)
{
    frame_scenes_.push_back(std::move(scene));
}

void engine::Renderer::add_feature(std::shared_ptr<RenderFeature> feature, RenderStage stage, int order)
{
    assert(feature);
    const auto position = std::upper_bound(features_.begin(), features_.end(), std::pair{ stage, order },
        [](const auto& key, const FeatureEntry& entry) { return key < std::pair{ entry.stage, entry.order }; });
    features_.insert(position, FeatureEntry{ std::move(feature), stage, order });
}

bool engine::Renderer::read_entity_id(std::uint32_t x, std::uint32_t y, std::uint32_t& out_entity_id)
{
    if (!entity_id_texture_ || x >= entity_id_desc_.width || y >= entity_id_desc_.height)
    {
        return false;
    }
    readback_targets_.set_color_attachment(0, entity_id_texture_.get());
    const auto pixels = readback_targets_.read_pixels(0, { x, y, 1, 1 }, DataLayout::eR_U32);
    readback_targets_.set_color_attachment(0, nullptr);
    RenderTargetSet::bind_default();

    assert(pixels.size() == sizeof(std::uint32_t));
    std::memcpy(&out_entity_id, pixels.data(), sizeof(std::uint32_t));
    return true;
}

engine::Renderer::SceneGpu& engine::Renderer::get_scene_gpu(std::size_t scene_index)
{
    while (scene_gpu_.size() <= scene_index)
    {
        scene_gpu_.push_back(std::make_unique<SceneGpu>());
    }
    return *scene_gpu_[scene_index];
}

void engine::Renderer::prepare_gpu_data()
{
    ENGINE_PROFILE_SECTION;
    frame_views_.clear();
    for (std::size_t scene_index = 0; scene_index < frame_scenes_.size(); scene_index++)
    {
        const auto& scene = frame_scenes_[scene_index];
        auto& gpu = get_scene_gpu(scene_index);

        {
            BufferMapContext<SceneGpuData, UniformBuffer> scene_ubo(gpu.scene_ubo, false, true);
            *scene_ubo.data = scene.scene_gpu;
        }

        {
            const auto required_size = std::max(scene.lights.size(), INITIAL_LIGHTS_CAPACITY) * sizeof(LightGpuData);
            if (ensure_buffer_capacity(gpu.lights_ssbo, required_size))
            {
                gpu.lights_ptr = gpu.lights_ssbo.map(false, true);
            }
            assert(gpu.lights_ptr != nullptr);
            if (!scene.lights.empty())
            {
                std::memcpy(gpu.lights_ptr, scene.lights.data(), scene.lights.size() * sizeof(LightGpuData));
            }
        }

        if (!scene.debug_lines.empty())
        {
            const auto required_size = std::max(scene.debug_lines.size(), INITIAL_DEBUG_LINES_CAPACITY) * sizeof(DebugLineGpuData);
            ensure_buffer_capacity(gpu.debug_lines_ssbo, required_size);
            BufferMapContext<DebugLineGpuData, ShaderStorageBuffer> lines(gpu.debug_lines_ssbo, false, true);
            std::memcpy(lines.data, scene.debug_lines.data(), scene.debug_lines.size() * sizeof(DebugLineGpuData));
        }

        while (gpu.view_ubos.size() < scene.views.size())
        {
            gpu.view_ubos.push_back(std::make_unique<UniformBuffer>(sizeof(CameraGpuData)));
        }
        for (std::size_t view_index = 0; view_index < scene.views.size(); view_index++)
        {
            {
                BufferMapContext<CameraGpuData, UniformBuffer> camera_ubo(*gpu.view_ubos[view_index], false, true);
                *camera_ubo.data = scene.views[view_index];
            }
            frame_views_.push_back(FrameView{
                .scene = &scene,
                .camera = &scene.views[view_index],
                .camera_ubo = gpu.view_ubos[view_index].get(),
                .scene_ubo = &gpu.scene_ubo,
                .lights_ssbo = &gpu.lights_ssbo,
                .debug_lines_ssbo = &gpu.debug_lines_ssbo });
        }
    }
}

void engine::Renderer::render_frame()
{
    ENGINE_PROFILE_SECTION;
    const auto window_size = rdx_.get_window_size_in_pixels();
    if (window_size.width <= 0 || window_size.height <= 0)
    {
        return;  // minimized
    }
    const auto width = static_cast<std::uint32_t>(window_size.width);
    const auto height = static_cast<std::uint32_t>(window_size.height);

    prepare_gpu_data();

    backend_.new_frame();
    graph_.reset();

    FrameContext frame{ .graph = graph_, .rdx = rdx_, .width = width, .height = height, .views = frame_views_ };
    frame.backbuffer = graph_.import_backbuffer("Backbuffer", width, height);

    add_scene_passes(frame);
    add_features_passes(frame, RenderStage::ePostProcess);
    add_present_pass(frame);
    add_features_passes(frame, RenderStage::eOverlay);

    graph_.extract_texture(frame.entity_id, &entity_id_texture_);
    entity_id_desc_ = rg::TextureDesc{ width, height, DataLayout::eR_U32 };

    graph_.compile();
    if (graph_.get_messages() != last_graph_messages_)
    {
        last_graph_messages_ = graph_.get_messages();
        for (const auto& message : last_graph_messages_)
        {
            log::log(log::LogLevel::eError, fmt::format("[RenderGraph] {}\n", message));
        }
    }
    graph_.execute(backend_);
}

void engine::Renderer::add_scene_passes(FrameContext& frame)
{
    const auto desc_rgba = rg::TextureDesc{ frame.width, frame.height, DataLayout::eRGBA_U8 };
    const auto desc_id = rg::TextureDesc{ frame.width, frame.height, DataLayout::eR_U32 };
    const auto desc_depth = rg::TextureDesc{ frame.width, frame.height, DataLayout::eDEPTH24_STENCIL8_U32 };

    // Scene targets are cleared once, passes of every camera draw on top of them.
    const auto& targets = frame.graph.add_pass<SceneTargetsPassData>("Clear Scene Targets",
        [&](rg::Builder& builder, SceneTargetsPassData& data)
        {
            data.color = builder.write_color(0, builder.create_texture("SceneColor", desc_rgba), rg::LoadOp::eClear, rg::ClearValue{ .color = { 0.0f, 0.0f, 0.0f, 1.0f } });
            data.entity_id = builder.write_color(1, builder.create_texture("EntityId", desc_id), rg::LoadOp::eClear, rg::ClearValue{ .uint_value = ENGINE_INVALID_GAME_OBJECT_ID });
            data.depth = builder.write_depth(builder.create_texture("SceneDepth", desc_depth), rg::LoadOp::eClear);
        },
        [](const SceneTargetsPassData&, rg::PassContext&) {});

    frame.scene_color = targets.color;
    frame.entity_id = targets.entity_id;
    frame.scene_depth = targets.depth;

    std::size_t view_counter = 0;
    for (const auto& view : frame.views)
    {
        const auto suffix = fmt::format(" [View {}]", view_counter++);

        frame.graph.add_pass(fmt::format("Forward{}", suffix),
            [&](rg::Builder& builder, rg::NoPassData&)
            {
                builder.write_color(0, frame.scene_color, rg::LoadOp::eLoad);
                builder.write_color(1, frame.entity_id, rg::LoadOp::eLoad);
                builder.write_depth(frame.scene_depth, rg::LoadOp::eLoad);
            },
            [this, view](const rg::NoPassData&, rg::PassContext&)
            {
                draw_meshes(view);
            });

        // Sprites don't write entity ids, so they are drawn without that attachment.
        if (!view.scene->sprites.empty())
        {
            frame.graph.add_pass(fmt::format("Sprites{}", suffix),
                [&](rg::Builder& builder, rg::NoPassData&)
                {
                    builder.write_color(0, frame.scene_color, rg::LoadOp::eLoad);
                    builder.write_depth(frame.scene_depth, rg::LoadOp::eLoad);
                },
                [this, view](const rg::NoPassData&, rg::PassContext&)
                {
                    draw_sprites(view);
                });
        }

        if (!view.scene->debug_lines.empty())
        {
            frame.graph.add_pass(fmt::format("Physics Debug Lines{}", suffix),
                [&](rg::Builder& builder, rg::NoPassData&)
                {
                    builder.write_color(0, frame.scene_color, rg::LoadOp::eLoad);
                    builder.write_depth(frame.scene_depth, rg::LoadOp::eLoad);
                },
                [this, view](const rg::NoPassData&, rg::PassContext&)
                {
                    draw_debug_lines(view);
                });
        }
    }
}

void engine::Renderer::add_present_pass(FrameContext& frame)
{
    frame.graph.add_pass<PresentPassData>("Present",
        [&](rg::Builder& builder, PresentPassData& data)
        {
            data.scene_color = builder.read(frame.scene_color);
            builder.write_color(0, frame.backbuffer, rg::LoadOp::eDontCare);
        },
        [this](const PresentPassData& data, rg::PassContext& ctx)
        {
            rdx_.set_depth_test(false);
            rdx_.set_cull_face(false);
            shader_full_screen_quad_.bind();
            shader_full_screen_quad_.set_texture_with_sampler("screen_texture", &ctx.texture(data.scene_color));
            geometry_full_screen_quad_.bind();
            geometry_full_screen_quad_.draw(Geometry::Mode::eTriangles);
        });
}

void engine::Renderer::add_features_passes(FrameContext& frame, RenderStage stage)
{
    for (const auto& entry : features_)
    {
        if (entry.stage == stage && entry.feature->is_enabled())
        {
            entry.feature->add_passes(frame);
        }
    }
}

void engine::Renderer::draw_meshes(const FrameView& view)
{
    ENGINE_PROFILE_SECTION_N("Renderer::draw_meshes");
    const auto& scene = *view.scene;
    view.lights_ssbo->bind(static_cast<std::uint32_t>(ShaderBindingSlot::LIGHTS));

    for (const auto& item : scene.static_meshes)
    {
        const auto ctx = MaterialStaticGeometryLit::DrawContext{
            .entity_id = item.entity_id,
            .camera = *view.camera_ubo,
            .scene = *view.scene_ubo,
            .model_matrix = glm::value_ptr(item.model),
            .color_diffuse = glm::value_ptr(item.diffuse_color),
            .shininess = item.shininess,
            .texture_diffuse = *item.texture_diffuse,
            .texture_specular = *item.texture_specular };
        material_static_geometry_lit_.draw(*item.geometry, ctx);
    }

    for (const auto& item : scene.skinned_meshes)
    {
        auto ctx = MaterialSkinnedGeometryLit::DrawContext{
            .entity_id = item.entity_id,
            .camera = *view.camera_ubo,
            .scene = *view.scene_ubo,
            .model_matrix = glm::value_ptr(item.model),
            .color_diffuse = glm::value_ptr(item.diffuse_color),
            .shininess = item.shininess,
            .texture_diffuse = *item.texture_diffuse,
            .texture_specular = *item.texture_specular };
        ctx.bone_transforms = std::span<const glm::mat4>(scene.skinning_matrices).subspan(item.skinning_matrices_offset, item.skinning_matrices_count);
        material_skinned_geometry_lit_.draw(*item.geometry, ctx);
    }
}

void engine::Renderer::draw_sprites(const FrameView& view)
{
    ENGINE_PROFILE_SECTION_N("Renderer::draw_sprites");
    for (const auto& item : view.scene->sprites)
    {
        const auto ctx = MaterialSprite::DrawContext{
            .camera = *view.camera_ubo,
            .scene = *view.scene_ubo,
            .world_position = item.world_position,
            .scale = item.scale };
        material_sprite_.draw(ctx);
    }
}

void engine::Renderer::draw_debug_lines(const FrameView& view)
{
    ENGINE_PROFILE_SECTION_N("Renderer::draw_debug_lines");
    const auto& scene = *view.scene;

    shader_debug_lines_.bind();
    shader_debug_lines_.set_uniform_mat_f4("view", { glm::value_ptr(view.camera->view), 16 });
    shader_debug_lines_.set_uniform_mat_f4("projection", { glm::value_ptr(view.camera->projection), 16 });
    view.debug_lines_ssbo->bind(static_cast<std::uint32_t>(ShaderBindingSlot::DEBUG_PHYSICS));
    geometry_debug_line_.bind();
    geometry_debug_line_.draw_instances(Geometry::Mode::eLines, static_cast<std::uint32_t>(scene.debug_lines.size()));
}
