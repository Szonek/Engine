#pragma once
#include "engine.h"
#include "animation.h"
#include "graphics.h"

#include <map>
#include <span>
#include <cstdint>
#include <vector>
#include <array>
#include <string>

namespace engine
{
struct GeometryInfo
{
    std::vector<engine_vertex_attribute_t> verticies;
    std::vector<std::uint32_t> indicies;
    std::int32_t material_index = -1;
};

struct TextureInfo
{
    std::string name;
    std::uint32_t width;
    std::uint32_t height;
    engine_data_layout_t layout;
    std::vector<std::byte> data;
};

struct MaterialInfo
{
    std::string name;
    std::array<float, 4> diffuse_factor;
    TextureInfo diffuse_texture;
};

struct AnimationInfo
{
    std::string name;
    AnimationClipData clip;
};

struct SkinJointInfo
{
    using TypeIdx = std::int32_t;
    TypeIdx idx = -1;
    std::vector<TypeIdx> childrens{};
    glm::mat4 inverse_bind_matrix{ 1.0f };
};

struct SkinInfo
{
    std::map<SkinJointInfo::TypeIdx, SkinJointInfo> joints;
};

struct ModelInfo
{
    std::vector<GeometryInfo> geometries;
    std::vector<MaterialInfo> materials;
    std::vector<AnimationInfo> animations;
    std::vector<SkinInfo> skins;
};


ModelInfo parse_gltf_data_from_memory(std::span<const std::uint8_t> data);
} // namespace engine>
