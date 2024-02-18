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
struct GeometryInfo
{
    engine_vertex_attributes_layout_t vertex_laytout{};
    std::vector<std::byte> vertex_data;
    std::int32_t vertex_count = 0;
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

struct AnimationChannelInfo
{
    engine_animation_channel_type_t type = ENGINE_ANIMATION_CHANNEL_TYPE_COUNT;
    std::vector<float> timestamps;
    std::vector<float> data;
    // target_node_idx is index of joint if animation is for skeleton
    std::int32_t target_node_idx = -1;
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

struct ModelInfo
{
    std::vector<GeometryInfo> geometries;
    std::vector<MaterialInfo> materials;
    std::vector<AnimationClipInfo> animations;
    std::vector<SkinInfo> skins;
};


ModelInfo parse_gltf_data_from_memory(std::span<const std::uint8_t> data);
} // namespace engine>
