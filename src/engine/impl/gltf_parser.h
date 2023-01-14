#pragma once
#include <span>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include "engine.h"

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

struct ModelInfo
{
    std::vector<GeometryInfo> geometries;
    std::vector<MaterialInfo> materials;
};

ModelInfo parse_gltf_data_from_memory(std::span<const std::uint8_t> data);
} // namespace engine
