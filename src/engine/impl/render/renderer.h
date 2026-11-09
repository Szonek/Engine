#pragma once
#include "render_feature.h"
#include "render_graph.h"
#include "render_graph_gl_backend.h"
#include "render_scene_data.h"
#include "render_settings.h"
#include "ssao_pass.h"
#include "../graphics.h"
#include "../material.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace engine
{
// Builds and executes the render graph of a frame.
//
// Frame flow:
//   Renderer::begin_frame()                   - start of the frame
//   Scene::update() -> submit_scene(...)      - game world describes what to draw (SceneRenderData), no GL calls
//   Renderer::render_frame()                  - uploads GPU data, declares passes, executes the graph:
//
//       Clear Scene Targets
//       for each submitted scene, for each enabled camera:
//           Shadow Map      (depth of meshes seen from the directional light)
//           Depth Pre-pass  (depth of meshes seen from the camera)
//           SSAO, SSAO Blur (ambient occlusion computed from the depth)
//           Forward (static + skinned meshes, uses the shadow map and the ambient occlusion) -> Sprites
//           -> Transparent (particles, sorted back to front; only when there are some) -> Debug Lines -> Debug Lines Pick
//       RenderStage::ePostProcess features    (i.e. editor outline)
//       Present                               (scene color -> scene output: the backbuffer or the texture set with set_scene_output())
//       RenderStage::eOverlay features        (i.e. game UI, drawn onto the scene output)
//       RenderStage::eScreen features         (i.e. the editor, drawn onto the backbuffer)
//
// New rendering can be added without touching the code above by implementing a RenderFeature and registering it with add_feature().
class Renderer
{
public:
    explicit Renderer(RenderContext& rdx);
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void begin_frame();
    void submit_scene(SceneRenderData&& scene);
    void render_frame();

    // Where the scene image is presented. nullptr (default): the backbuffer, the scene is rendered at the window size.
    // A texture (RGBA8): the scene is rendered at the texture's size, i.e. for the editor to show it in a panel.
    void set_scene_output(std::shared_ptr<Texture2D> texture) { scene_output_ = std::move(texture); }

    // Features of the same stage are executed in ascending order, ties in registration order.
    void add_feature(std::shared_ptr<RenderFeature> feature, RenderStage stage, int order = 0);

    // The entity ids rendered during the previous frame in the square of pixels `radius` around (x, y) (0,0 is the left
    // bottom corner), cut to the image, row by row from the bottom: Forward writes the meshes', Debug Lines Pick the
    // owners' of debug lines.
    struct EntityIdArea
    {
        std::vector<std::uint32_t> ids;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        // where (x, y) is in the area
        std::uint32_t center_x = 0;
        std::uint32_t center_y = 0;
        // the owners of the debug lines of the same frame (sorted, each once), for pick_game_object
        std::vector<std::uint32_t> owners;
    };
    // nullopt if there is no such data (nothing was rendered yet or the pixel is outside)
    std::optional<EntityIdArea> read_entity_ids(std::uint32_t x, std::uint32_t y, std::uint32_t radius);

    RenderContext& get_context() { return rdx_; }
    // Shadows, ambient occlusion... Changes are applied to the next rendered frame.
    RenderSettings& get_settings() { return settings_; }
    // Graph of the last rendered frame, for debugging/visualization.
    const rg::Graph& get_last_frame_graph() const { return graph_; }

private:
    // GPU buffers of a single camera
    struct ViewGpu
    {
        UniformBuffer camera_ubo{ sizeof(CameraGpuData) };
        UniformBuffer lighting_ubo{ sizeof(ViewLightingGpuData) };
        // the directional light as a camera: what the shadow map of this view is rendered with
        UniformBuffer shadow_camera_ubo{ sizeof(CameraGpuData) };
    };

    // Persistently mapped SSBOs written every frame. The GPU may still be reading the previous frames' data,
    // so each frame writes its own copy and waits (fence) before reusing one, FRAMES_IN_FLIGHT frames later.
    static constexpr std::size_t FRAMES_IN_FLIGHT = 3;
    struct MappedSsbo
    {
        ShaderStorageBuffer ssbo;
        void* ptr = nullptr;
    };
    struct FrameSsbos
    {
        MappedSsbo lights;
        MappedSsbo skinning;
        // the scene's particles, once per view (each sorted for its camera)
        MappedSsbo particles;
        GpuFence fence;
    };
    // one instanced draw of the Transparent pass: a batch's particles, in the view's sorted range of the particle SSBO
    struct ParticleDraw
    {
        std::uint32_t first = 0;
        const ParticleBatch* batch = nullptr;
    };

    // GPU buffers of a single submitted scene
    struct SceneGpu
    {
        UniformBuffer scene_ubo{ sizeof(SceneGpuData) };
        std::array<FrameSsbos, FRAMES_IN_FLIGHT> frames;
        ShaderStorageBuffer debug_lines_ssbo;
        std::vector<std::unique_ptr<ViewGpu>> views;
    };

    struct FeatureEntry
    {
        std::shared_ptr<RenderFeature> feature;
        RenderStage stage;
        int order;
    };

    struct SceneTargetsPassData
    {
        rg::TextureHandle color;
        rg::TextureHandle entity_id;
        rg::TextureHandle depth;
    };

    struct ShadowPassData
    {
        rg::TextureHandle shadow_map;
    };

    // invalid handles: the view has no shadow / no ambient occlusion
    struct ForwardPassData
    {
        rg::TextureHandle shadow_map;
        rg::TextureHandle ambient_occlusion;
    };

    struct PresentPassData
    {
        rg::TextureHandle scene_color;
    };

    SceneGpu& get_scene_gpu(std::size_t scene_index);
    void prepare_gpu_data();
    void add_scene_passes(FrameContext& frame);
    void add_present_pass(FrameContext& frame);
    void add_features_passes(FrameContext& frame, RenderStage stage);

    void draw_meshes(const FrameView& view);
    // camera_ubo: camera of the view or of the light
    void draw_meshes_depth_only(const FrameView& view, const UniformBuffer& camera_ubo);
    void draw_sprites(const FrameView& view);
    void draw_debug_lines(const FrameView& view, Shader& shader);
    // sorts the scene's particles back to front for the view's camera into `out` (the view's part of the particle
    // SSBO, from `first`) and lists the draws, farthest batch first
    static void sort_particles(const SceneRenderData& scene, const CameraGpuData& camera, std::uint32_t first, ParticleGpuData* out, std::vector<ParticleDraw>& draws);
    void draw_particles(const FrameView& view, const std::vector<ParticleDraw>& draws, const Texture2D& shadow_map, const Texture2D* depth_copy);

private:
    RenderContext& rdx_;
    GlGraphBackend backend_;
    rg::Graph graph_;
    RenderSettings settings_;

    std::vector<SceneRenderData> frame_scenes_;
    std::vector<std::unique_ptr<SceneGpu>> scene_gpu_;
    // selects SceneGpu::frames used by the current frame
    std::size_t frame_slot_ = 0;
    std::vector<FrameView> frame_views_;
    // the Transparent pass's draws of each view of frame_views_
    std::vector<std::vector<ParticleDraw>> frame_particle_draws_;
    std::vector<FeatureEntry> features_;
    std::vector<std::string> last_graph_messages_;
    std::shared_ptr<Texture2D> scene_output_;

    // entity ids of the previous frame, used for picking
    std::shared_ptr<Texture2D> entity_id_texture_;
    rg::TextureDesc entity_id_desc_;
    // the owners of the debug lines rendered into entity_id_texture_ (unique_owners)
    std::vector<std::uint32_t> entity_id_owners_;
    RenderTargetSet readback_targets_;

    MaterialStaticGeometryLit material_static_geometry_lit_;
    MaterialSkinnedGeometryLit material_skinned_geometry_lit_;
    MaterialStaticGeometryDepthOnly material_static_geometry_depth_only_;
    MaterialSkinnedGeometryDepthOnly material_skinned_geometry_depth_only_;
    MaterialSprite material_sprite_;

    Shader shader_debug_lines_;
    // the lines' owners into the entity ids, for picking
    Shader shader_debug_lines_pick_;
    Shader shader_particles_;
    // the scene's depth into a colour texture (soft particles)
    Shader shader_depth_copy_;
    // six vertices without data: a quad made in the vertex shader
    Geometry geometry_particle_quad_;
    Geometry geometry_debug_line_;
    Shader shader_full_screen_quad_;
    Geometry geometry_full_screen_quad_;

    SsaoPass ssao_pass_;
    // bound in place of the shadow map / ambient occlusion when they are disabled
    Texture2D white_texture_;
};

}  // namespace engine
