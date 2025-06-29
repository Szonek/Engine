#pragma once
#include "mesh_defs.h"
#include <span>
#include <cstdint>
#include <string>

namespace engine
{
// base dir to search for assets (i.e. images)
ModelDesc parse_gltf_data_from_memory(std::span<const std::uint8_t> data, const std::string& base_dir);
} // namespace engine>
