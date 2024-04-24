#pragma once
#include "engine.h"
#include "graphics.h"
#include "math_helpers.h"

#include <map>
#include <span>
#include <cstdint>
#include <vector>
#include <array>
#include <string>

namespace engine
{

inline static const std::int32_t INVALID_VALUE = -1;

struct GeometryInfo
{
    engine_vertex_attributes_layout_t vertex_laytout{};
    std::vector<std::byte> vertex_data;
    std::int32_t vertex_count = 0;
    std::vector<std::uint32_t> indicies;
    std::int32_t material_index = INVALID_VALUE;
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
    std::int32_t diffuse_texture = INVALID_VALUE;
};

enum class AnimationChannelType
{
    eUnknown,
    eTranslation,
    eRotation,
    eScale
};

struct AnimationChannelInfo
{
    AnimationChannelType type = AnimationChannelType::eUnknown;
    std::vector<float> timestamps;
    std::vector<float> data;
    std::int32_t target_node_idx = INVALID_VALUE;
};

struct AnimationClipInfo
{
    std::string name;
    std::vector<AnimationChannelInfo> channels;
};

struct BoneInfo
{
    std::int32_t target_node_idx = INVALID_VALUE;
    glm::mat4 inverse_bind_matrix;
};

struct SkinInfo
{
    std::string name = "";
    std::vector<BoneInfo> bones;
};


struct ModelNode
{
    std::string name = "";
    std::int32_t index = INVALID_VALUE;
    std::int32_t mesh = INVALID_VALUE;
    std::int32_t skin = INVALID_VALUE;
    std::int32_t joint = INVALID_VALUE;
    std::shared_ptr<ModelNode> parent = nullptr; // shared_ptr to have pointer stability while erasing nodes
    std::vector<std::shared_ptr<ModelNode>> children = {};

    glm::vec3 translation;
    glm::vec3 scale = glm::vec3(1.0f);
    glm::quat rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
};


struct ModelInfo
{
    std::vector<std::shared_ptr<ModelNode>> nodes;
    std::vector<GeometryInfo> geometries;
    std::vector<MaterialInfo> materials;
    std::vector<TextureInfo> textures;
    std::vector<AnimationClipInfo> animations;
    std::vector<SkinInfo> skins;
};


ModelInfo parse_gltf_data_from_memory(std::span<const std::uint8_t> data);
} // namespace engine>
