#pragma once
#include <fmt/format.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace engine
{
// Asset ID (and atlas name) of a resource that lives inside a source file, e.g. a mesh of a glTF model:
// "<source>#<name>", or "<source>#<unnamed_kind>_<index>" when the resource has no name.
// It's unique as long as the resource names are unique within the source file.
inline std::string make_asset_id(std::string_view source, std::string_view name, std::string_view unnamed_kind, std::size_t index)
{
    if (name.empty())
    {
        return fmt::format("{}#{}_{}", source, unnamed_kind, index);
    }
    return fmt::format("{}#{}", source, name);
}
}  // namespace engine
