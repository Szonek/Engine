#pragma once
#include "mesh_defs.h"
#include <span>
#include <cstdint>
#include <string>

namespace engine
{
// base dir to search for assets (i.e. images)
// source_name (usually the model file name) prefixes geometry and embedded texture names ("<source_name>#<mesh name>"),
// so they stay unique when several models are loaded
ModelDesc parse_gltf_data_from_memory(std::span<const std::uint8_t> data, const std::string& base_dir, const std::string& source_name);
} // namespace engine>
