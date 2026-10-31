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

// Shadow, ambient occlusion and stylized lighting parameters of one camera. Layout has to match ViewLightingData in shadow_helpers.h.
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
    float shadow_hard_edges = 0.0f;
    float pad1_ = 0.0f;
    // stylized lighting (StylizedLightingSettings), ignored while stylized_enabled is 0
    glm::vec4 stylized_sky_ambient{ 1.0f };     // rgb
    glm::vec4 stylized_ground_ambient{ 1.0f };  // rgb
    glm::vec4 stylized_shadow_color{ 0.0f };    // rgb
    glm::vec4 stylized_rim_color{ 0.0f };       // rgb: color * strength
    float stylized_enabled = 0.0f;
    float stylized_bands = 1.0f;
    float stylized_band_softness = 1.0f;
    float stylized_rim_power = 1.0f;
    float stylized_rim_static_meshes = 0.0f;
    float pad2_ = 0.0f;
    float pad3_ = 0.0f;
    float pad4_ = 0.0f;
};
static_assert(sizeof(ViewLightingGpuData) == 208, "has to match the std140 size of ViewLightingData in shadow_helpers.h");

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
    std::int32_t life_time = 0;
};

struct StaticMeshDrawItem
{
    std::uint32_t entity_id = 0;
    const Geometry* geometry = nullptr;
    glm::mat4 model{ 1.0f };
    // world position of the root of the object's hierarchy: everything in a hierarchy (a character, its parts and its
    // weapon) is snapped to the pixel grid by the same offset, so it stays together (pixel_art.h)
    glm::vec3 snap_origin{ 0.0f };
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

// Geometry rendered into the selection mask (editor outline effect).
struct OutlineDrawItem
{
    const Geometry* geometry = nullptr;
    glm::mat4 model{ 1.0f };
    glm::vec3 snap_origin{ 0.0f };  // see StaticMeshDrawItem::snap_origin
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
};

}  // namespace engine
