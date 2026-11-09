#pragma once
#include "../graphics.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace engine
{
// Plain data describing what to draw. Scene (game world) fills it every update and submits it to the Renderer.
// It deliberately contains copies of everything the passes need (matrices, colors, ...), not pointers into ECS components,
// so the renderer doesn't depend on the scene's state. Geometry/Texture pointers are owned by the Application and must stay valid until the end of the frame.

// GPU layouts (std140 / std430), have to match shaders.
struct CameraGpuData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 position;
    float pad_;
};

struct SceneGpuData
{
    std::uint32_t direction_light_count = 0;
    std::uint32_t point_light_count = 0;
    std::uint32_t spot_light_count = 0;
    float pad3_;
};

// Shadow and ambient occlusion parameters of one camera. Layout has to match ViewLightingData in shadow_helpers.h.
struct ViewLightingGpuData
{
    // world space -> shadow map clip space
    glm::mat4 shadow_view_projection{ 1.0f };
    float shadow_enabled = 0.0f;
    float shadow_depth_bias = 0.0f;
    float shadow_normal_offset = 0.0f;  // world units
    float shadow_strength = 0.0f;
    float shadow_fade_start = 0.0f;     // distance from the camera at which the shadow starts to fade out
    float shadow_fade_end = 0.0f;
    std::int32_t shadow_pcf_radius = 0;
    float ssao_enabled = 0.0f;
    float ssao_direct_light_occlusion = 0.0f;
    std::uint32_t debug_view = 0;       // RenderDebugView
    float pad0_ = 0.0f;
    float pad1_ = 0.0f;
};

struct LightGpuData
{
    glm::vec3 position;
    float cutoff;
    glm::vec3 direction;
    float outer_cutoff;
    float constant;
    float linear;
    float quadratic;
    float pad0_;
    glm::vec3 ambient;
    float pad1_;
    glm::vec3 diffuse;
    float pad2_;
    glm::vec3 specular;
    float pad3_;
};

struct DebugLineGpuData
{
    glm::vec3 from;
    float pad0;
    glm::vec3 to;
    float pad1;
    glm::vec3 color;
    // the game object a click on the line selects in the editor (DebugDrawLine::owner), 0 for none
    std::uint32_t owner = 0;
};

struct StaticMeshDrawItem
{
    std::uint32_t entity_id = 0;
    const Geometry* geometry = nullptr;
    glm::mat4 model{ 1.0f };
    glm::vec4 diffuse_color{ 1.0f };
    float shininess = 0.0f;
    const Texture2D* texture_diffuse = nullptr;
    const Texture2D* texture_specular = nullptr;
};

struct SkinnedMeshDrawItem : StaticMeshDrawItem
{
    // range in SceneRenderData::skinning_matrices
    std::uint32_t skinning_matrices_offset = 0;
    std::uint32_t skinning_matrices_count = 0;
};

struct SpriteDrawItem
{
    glm::vec3 world_position{ 0.0f };
    glm::vec3 scale{ 1.0f };
};

// One particle as the Transparent pass draws it (std430, ParticlePacket in particles.vs): a camera-facing quad.
struct ParticleGpuData
{
    glm::vec3 position;  // world space
    float size;          // the quad's side, world units
    glm::vec4 color;     // RGBA
    float rotation;      // radians, around the view axis
    float erosion;       // the alpha threshold, 0: none
    float frame;         // the flipbook frame (fractional: blended into the next)
    float pad0_ = 0.0f;
};

// how a particle batch is blended (ParticleBlendMode)
enum class ParticleBlend : std::uint32_t
{
    eAlpha = 0,
    ePremultiplied = 1,
    eAdditive = 2,
};

// The live particles of one emitter of one playing effect, drawn with one instanced draw: a range of
// SceneRenderData::particles. Sorted back to front with the others (by `center`) and within itself per camera.
struct ParticleBatch
{
    std::uint32_t offset = 0;
    std::uint32_t count = 0;
    // nullptr: the built-in soft circle
    const Texture2D* texture = nullptr;
    // the texture holds a smoke puff (R brightness, G erosion noise, A alpha) instead of a colour
    bool packed_channels = false;
    std::uint32_t flipbook_columns = 1;
    std::uint32_t flipbook_rows = 1;
    bool flipbook_blend = false;
    float erosion_softness = 0.1f;
    ParticleBlend blend = ParticleBlend::eAlpha;
    // lit by the directional light (with its shadow), with `light_wrap` of wrap-around
    bool lit = false;
    float light_wrap = 0.5f;
    // > 0: fades near the geometry behind (needs a copy of the scene's depth)
    float soft_distance = 0.0f;
    std::int32_t sort_order = 0;
    // where the batch is, for sorting the batches: the emitter's position
    glm::vec3 center{ 0.0f };
};

// Geometry rendered into the selection mask (editor outline effect).
struct OutlineDrawItem
{
    const Geometry* geometry = nullptr;
    glm::mat4 model{ 1.0f };
};

struct SceneRenderData
{
    // one entry for each enabled camera
    std::vector<CameraGpuData> views;

    SceneGpuData scene_gpu;
    // directional lights first, then point lights, then spot lights (the order shaders expect)
    std::vector<LightGpuData> lights;

    std::vector<StaticMeshDrawItem> static_meshes;
    std::vector<SkinnedMeshDrawItem> skinned_meshes;
    std::vector<glm::mat4> skinning_matrices;
    std::vector<SpriteDrawItem> sprites;

    std::vector<OutlineDrawItem> outline_items;
    std::vector<DebugLineGpuData> debug_lines;
    // the particles of every playing effect (ranges of it are ParticleBatch), drawn by the Transparent pass
    std::vector<ParticleGpuData> particles;
    std::vector<ParticleBatch> particle_batches;
};

}  // namespace engine
