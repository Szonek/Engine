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

struct GeometryDesc
{
    std::string name = "";
    engine_vertex_attributes_layout_t vertex_laytout{};
    std::vector<std::byte> vertex_data;
    std::int32_t vertex_count = 0;
    std::vector<std::uint32_t> indicies;
};

struct TextureDesc
{
    std::string name;
    std::uint32_t width;
    std::uint32_t height;
    engine_data_layout_t layout;
    std::vector<std::byte> data;
};

struct MaterialDesc
{
    std::string name;
    glm::vec4 diffuse_factor;
    std::int32_t diffuse_texture = INVALID_VALUE;
};

enum class AnimationChannelType
{
    eUnknown,
    eTranslation,
    eRotation,
    eScale
};

struct AnimationChannelDesc
{
    AnimationChannelType type = AnimationChannelType::eUnknown;
    std::vector<float> timestamps;
    std::vector<float> data;
    std::int32_t target_node_idx = INVALID_VALUE;
};

struct AnimationClipDesc
{
    std::string name;
    std::vector<AnimationChannelDesc> channels;
};

struct BoneDesc
{
    std::int32_t target_node_idx = INVALID_VALUE;
    glm::mat4 inverse_bind_matrix;
};

struct SkinDesc
{
    std::string name = "";
    std::vector<BoneDesc> bones;
};


struct ModelNodeDesc
{
    std::string name = "";
    std::int32_t index = INVALID_VALUE;
    std::int32_t mesh = INVALID_VALUE;
    std::int32_t material = INVALID_VALUE;
    std::int32_t skin = INVALID_VALUE;
    std::shared_ptr<ModelNodeDesc> parent = nullptr; // shared_ptr to have pointer stability while erasing nodes
    std::vector<std::shared_ptr<ModelNodeDesc>> children = {};

    glm::vec3 translation;
    glm::vec3 scale = glm::vec3(1.0f);
    glm::quat rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
};


struct ModelDesc
{
    std::vector<std::shared_ptr<ModelNodeDesc>> nodes;
    std::vector<GeometryDesc> geometries;
    std::vector<MaterialDesc> materials;
    std::vector<TextureDesc> textures;
    std::vector<AnimationClipDesc> animations;
    std::vector<SkinDesc> skins;
};


// base dir to search for assets (i.e. images)
ModelDesc parse_gltf_data_from_memory(std::span<const std::uint8_t> data, const std::string& base_dir);
} // namespace engine>
