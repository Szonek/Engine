#include "renderer.h"
#include "shadows.h"
#include "../logger.h"
#include "../profiler.h"
#include "../shaders_binding_slots.h"

#include <engine.h>

#include <fmt/format.h>

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>

namespace
{
constexpr std::size_t INITIAL_LIGHTS_CAPACITY = 1'000;
constexpr std::size_t INITIAL_SKINNING_MATRICES_CAPACITY = 4 * 1024;
constexpr std::size_t INITIAL_DEBUG_LINES_CAPACITY = 8 * 1024;
// shadow fades out over the last part of the shadow distance
constexpr float SHADOW_FADE_START = 0.85f;
constexpr std::uint32_t SHADOW_MAX_PCF_RADIUS = 4;
constexpr std::array<std::uint8_t, 4> WHITE_PIXEL = { 255, 255, 255, 255 };

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
    , ssao_pass_(rdx, geometry_full_screen_quad_)
    , white_texture_(1, 1, false, WHITE_PIXEL.data(), DataLayout::eRGBA_U8, TextureAddressClampMode::eClampToEdge)
{
}

engine::Renderer::~Renderer() = default;

void engine::Renderer::begin_frame()
{
    frame_scenes_.clear();
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

        // the GPU has to be done with the frame that used these buffers FRAMES_IN_FLIGHT frames ago before they are overwritten
        auto& frame_ssbos = gpu.frames[frame_slot_];
        frame_ssbos.fence.wait();

        {
            const auto required_size = std::max(scene.lights.size(), INITIAL_LIGHTS_CAPACITY) * sizeof(LightGpuData);
            if (ensure_buffer_capacity(frame_ssbos.lights.ssbo, required_size))
            {
                frame_ssbos.lights.ptr = frame_ssbos.lights.ssbo.map(false, true);
            }
            assert(frame_ssbos.lights.ptr != nullptr);
            if (!scene.lights.empty())
            {
                std::memcpy(frame_ssbos.lights.ptr, scene.lights.data(), scene.lights.size() * sizeof(LightGpuData));
            }
        }

        {
            // bone matrices of all skinned meshes, draws address their part with SkinnedMeshDrawItem::skinning_matrices_offset
            const auto required_size = std::max(scene.skinning_matrices.size(), INITIAL_SKINNING_MATRICES_CAPACITY) * sizeof(glm::mat4);
            if (ensure_buffer_capacity(frame_ssbos.skinning.ssbo, required_size))
            {
                frame_ssbos.skinning.ptr = frame_ssbos.skinning.ssbo.map(false, true);
            }
            assert(frame_ssbos.skinning.ptr != nullptr);
            if (!scene.skinning_matrices.empty())
            {
                std::memcpy(frame_ssbos.skinning.ptr, scene.skinning_matrices.data(), scene.skinning_matrices.size() * sizeof(glm::mat4));
            }
        }

        if (!scene.debug_lines.empty())
        {
            const auto required_size = std::max(scene.debug_lines.size(), INITIAL_DEBUG_LINES_CAPACITY) * sizeof(DebugLineGpuData);
            ensure_buffer_capacity(gpu.debug_lines_ssbo, required_size);
            BufferMapContext<DebugLineGpuData, ShaderStorageBuffer> lines(gpu.debug_lines_ssbo, false, true);
            std::memcpy(lines.data, scene.debug_lines.data(), scene.debug_lines.size() * sizeof(DebugLineGpuData));
        }

        while (gpu.views.size() < scene.views.size())
        {
            gpu.views.push_back(std::make_unique<ViewGpu>());
        }
        // the first directional light casts shadows (directional lights are at the beginning of the lights array)
        const bool has_shadow = settings_.shadows.enabled && scene.scene_gpu.direction_light_count > 0 && !scene.lights.empty();
        for (std::size_t view_index = 0; view_index < scene.views.size(); view_index++)
        {
            const auto& camera = scene.views[view_index];
            auto& view_gpu = *gpu.views[view_index];
            {
                BufferMapContext<CameraGpuData, UniformBuffer> camera_ubo(view_gpu.camera_ubo, false, true);
                *camera_ubo.data = camera;
            }

            ViewLightingGpuData lighting;
            lighting.ssao_enabled = settings_.ssao.enabled ? 1.0f : 0.0f;
            lighting.ssao_direct_light_occlusion = std::clamp(settings_.ssao.direct_light_occlusion, 0.0f, 1.0f);
            lighting.debug_view = static_cast<std::uint32_t>(settings_.debug_view);
            if (has_shadow)
            {
                // The shadow map follows the camera, so each view has its own.
                const auto shadow = compute_directional_shadow(camera.view, camera.projection, scene.lights.front().direction, settings_.shadows);
                {
                    BufferMapContext<CameraGpuData, UniformBuffer> shadow_camera_ubo(view_gpu.shadow_camera_ubo, false, true);
                    *shadow_camera_ubo.data = CameraGpuData{ .view = shadow.view, .projection = shadow.projection, .position = glm::vec3(0.0f), .pad_ = 0.0f };
                }
                lighting.shadow_view_projection = shadow.projection * shadow.view;
                lighting.shadow_enabled = 1.0f;
                lighting.shadow_depth_bias = settings_.shadows.depth_bias;
                lighting.shadow_normal_offset = settings_.shadows.normal_offset * shadow.world_texel_size;
                lighting.shadow_strength = std::clamp(settings_.shadows.strength, 0.0f, 1.0f);
                lighting.shadow_fade_start = settings_.shadows.distance * SHADOW_FADE_START;
                lighting.shadow_fade_end = settings_.shadows.distance;
                lighting.shadow_pcf_radius = static_cast<std::int32_t>(std::min(settings_.shadows.pcf_radius, SHADOW_MAX_PCF_RADIUS));
            }
            {
                BufferMapContext<ViewLightingGpuData, UniformBuffer> lighting_ubo(view_gpu.lighting_ubo, false, true);
                *lighting_ubo.data = lighting;
            }

            frame_views_.push_back(FrameView{
                .scene = &scene,
                .camera = &camera,
                .camera_ubo = &view_gpu.camera_ubo,
                .scene_ubo = &gpu.scene_ubo,
                .lights_ssbo = &frame_ssbos.lights.ssbo,
                .debug_lines_ssbo = &gpu.debug_lines_ssbo,
                .skinning_ssbo = &frame_ssbos.skinning.ssbo,
                .lighting_ubo = &view_gpu.lighting_ubo,
                .shadow_camera_ubo = &view_gpu.shadow_camera_ubo,
                .has_shadow = has_shadow });
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
    const auto window_width = static_cast<std::uint32_t>(window_size.width);
    const auto window_height = static_cast<std::uint32_t>(window_size.height);
    const auto width = scene_output_ ? scene_output_->get_width() : window_width;
    const auto height = scene_output_ ? scene_output_->get_height() : window_height;

    prepare_gpu_data();

    backend_.new_frame();
    graph_.reset();

    FrameContext frame{ .graph = graph_, .rdx = rdx_, .width = width, .height = height, .views = frame_views_, .full_screen_quad = &geometry_full_screen_quad_ };
    frame.backbuffer = graph_.import_backbuffer("Backbuffer", window_width, window_height);
    frame.scene_output = scene_output_
        ? graph_.import_texture("SceneOutput", scene_output_, rg::TextureDesc{ width, height, DataLayout::eRGBA_U8 })
        : frame.backbuffer;

    add_scene_passes(frame);
    add_features_passes(frame, RenderStage::ePostProcess);
    add_present_pass(frame);
    add_features_passes(frame, RenderStage::eOverlay);
    add_features_passes(frame, RenderStage::eScreen);

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

    // everything reading this frame's buffers has been submitted
    for (std::size_t scene_index = 0; scene_index < frame_scenes_.size(); scene_index++)
    {
        scene_gpu_[scene_index]->frames[frame_slot_].fence.signal();
    }
    frame_slot_ = (frame_slot_ + 1) % FRAMES_IN_FLIGHT;
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
        const bool is_first_view = view_counter == 0;
        const auto suffix = fmt::format(" [View {}]", view_counter++);

        // Depth of the scene seen from the directional light. The lit shader compares against it to find out what is in shadow.
        rg::TextureHandle shadow_map;
        if (view.has_shadow)
        {
            const auto shadow_map_size = std::max(settings_.shadows.map_size, 1u);
            const auto desc_shadow_map = rg::TextureDesc{ shadow_map_size, shadow_map_size, DataLayout::eDEPTH24_STENCIL8_U32 };
            shadow_map = frame.graph.add_pass<ShadowPassData>(fmt::format("Shadow Map{}", suffix),
                [&](rg::Builder& builder, ShadowPassData& data)
                {
                    data.shadow_map = builder.write_depth(builder.create_texture("ShadowMap", desc_shadow_map), rg::LoadOp::eClear);
                },
                [this, view](const ShadowPassData&, rg::PassContext&)
                {
                    draw_meshes_depth_only(view, *view.shadow_camera_ubo);
                }).shadow_map;
        }

        // Depth of the scene seen from the camera, rendered before any shading is done:
        //  - ambient occlusion is computed from it and it's needed by the Forward pass,
        //  - the Forward pass shades only the visible pixels (everything hidden fails the depth test).
        // Every camera starts from cleared depth (the first one gets it from Clear Scene Targets), so ambient occlusion
        // is never computed from depth rendered with another camera's projection.
        frame.graph.add_pass(fmt::format("Depth Pre-pass{}", suffix),
            [&](rg::Builder& builder, rg::NoPassData&)
            {
                builder.write_depth(frame.scene_depth, is_first_view ? rg::LoadOp::eLoad : rg::LoadOp::eClear);
            },
            [this, view](const rg::NoPassData&, rg::PassContext&)
            {
                draw_meshes_depth_only(view, *view.camera_ubo);
            });

        rg::TextureHandle ambient_occlusion;
        if (settings_.ssao.enabled)
        {
            ambient_occlusion = ssao_pass_.add_passes(frame.graph, frame.scene_depth, *view.camera, settings_.ssao, suffix);
        }

        frame.graph.add_pass<ForwardPassData>(fmt::format("Forward{}", suffix),
            [&](rg::Builder& builder, ForwardPassData& data)
            {
                if (shadow_map.is_valid())
                {
                    data.shadow_map = builder.read(shadow_map);
                }
                if (ambient_occlusion.is_valid())
                {
                    data.ambient_occlusion = builder.read(ambient_occlusion);
                }
                builder.write_color(0, frame.scene_color, rg::LoadOp::eLoad);
                builder.write_color(1, frame.entity_id, rg::LoadOp::eLoad);
                builder.write_depth(frame.scene_depth, rg::LoadOp::eLoad);
            },
            [this, view](const ForwardPassData& data, rg::PassContext& ctx)
            {
                const auto& shadow_map_texture = data.shadow_map.is_valid() ? ctx.texture(data.shadow_map) : white_texture_;
                const auto& ambient_occlusion_texture = data.ambient_occlusion.is_valid() ? ctx.texture(data.ambient_occlusion) : white_texture_;
                shadow_map_texture.bind_with_sampler(static_cast<std::uint32_t>(ShaderBindingSlot::TEXTURE_SHADOW_MAP));
                ambient_occlusion_texture.bind_with_sampler(static_cast<std::uint32_t>(ShaderBindingSlot::TEXTURE_SSAO));
                // depth is already there (Depth Pre-pass), so the visible geometry is exactly equal to it
                rdx_.set_depth_func(RenderContext::DepthFunc::eLessOrEqual);
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
            builder.write_color(0, frame.scene_output, rg::LoadOp::eDontCare);
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
    view.skinning_ssbo->bind(static_cast<std::uint32_t>(ShaderBindingSlot::SKINNING));
    view.lighting_ubo->bind(static_cast<std::uint32_t>(ShaderBindingSlot::VIEW_LIGHTING));

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
        const auto ctx = MaterialSkinnedGeometryLit::DrawContext{
            .entity_id = item.entity_id,
            .camera = *view.camera_ubo,
            .scene = *view.scene_ubo,
            .model_matrix = glm::value_ptr(item.model),
            .skinning_matrices_offset = item.skinning_matrices_offset,
            .color_diffuse = glm::value_ptr(item.diffuse_color),
            .shininess = item.shininess,
            .texture_diffuse = *item.texture_diffuse,
            .texture_specular = *item.texture_specular };
        material_skinned_geometry_lit_.draw(*item.geometry, ctx);
    }
}

void engine::Renderer::draw_meshes_depth_only(const FrameView& view, const UniformBuffer& camera_ubo)
{
    ENGINE_PROFILE_SECTION_N("Renderer::draw_meshes_depth_only");
    const auto& scene = *view.scene;
    view.skinning_ssbo->bind(static_cast<std::uint32_t>(ShaderBindingSlot::SKINNING));

    for (const auto& item : scene.static_meshes)
    {
        const auto ctx = MaterialStaticGeometryDepthOnly::DrawContext{
            .camera = camera_ubo,
            .model_matrix = glm::value_ptr(item.model) };
        material_static_geometry_depth_only_.draw(*item.geometry, ctx);
    }

    for (const auto& item : scene.skinned_meshes)
    {
        const auto ctx = MaterialSkinnedGeometryDepthOnly::DrawContext{
            .camera = camera_ubo,
            .model_matrix = glm::value_ptr(item.model),
            .skinning_matrices_offset = item.skinning_matrices_offset };
        material_skinned_geometry_depth_only_.draw(*item.geometry, ctx);
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
