#pragma once
#include "engine.h"
#include "animation.h"
#include "graphics.h"
#include "vertex_skinning.h"

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
    TextureInfo diffuse_texture;
};

struct AnimationChannelInfo
{
    engine_animation_channel_type_t type = ENGINE_ANIMATION_CHANNEL_TYPE_COUNT;
    std::vector<float> timestamps;
    std::vector<float> data;
    std::int32_t target_joint_idx = -1;
};

struct AnimationClipInfo
{
    std::string name;
    std::vector<AnimationChannelInfo> channels;
};

struct SkinInfo
{
    std::vector<SkinJointDesc> joints;
};


struct ModelNode
{

    std::string name = "";
    std::int32_t index = INVALID_VALUE;
    std::int32_t mesh = INVALID_VALUE;
    std::int32_t skin = INVALID_VALUE;
    std::int32_t joint = INVALID_VALUE;
    ModelNode* parent = nullptr;
    std::vector<ModelNode*> children = {};

    glm::vec3 translation;
    glm::vec3 scale = glm::vec3(1.0f);
    glm::quat rotation;
};


struct ModelInfo
{
    std::vector<ModelNode> nodes;
    std::vector<GeometryInfo> geometries;
    std::vector<MaterialInfo> materials;
    std::vector<AnimationClipInfo> animations;
    std::vector<SkinInfo> skins;
};


ModelInfo parse_gltf_data_from_memory(std::span<const std::uint8_t> data);
} // namespace engine>
