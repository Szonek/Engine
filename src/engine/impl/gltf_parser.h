#pragma once
#include <span>
#include <cstdint>
#include <vector>

#include "engine.h"

namespace engine
{
struct GeometryInfo
{
    std::vector<engine_vertex_attribute_t> verticies;
    std::vector<std::uint32_t> indicies;
};

struct ModelInfo
{
    std::vector<GeometryInfo> geometries;
};

ModelInfo parse_gltf_data_from_memory(std::span<const std::uint8_t> data);
} // namespace engine
