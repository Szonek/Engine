#include "editor_asset_browser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>
#include <system_error>

namespace
{
std::string to_lower(std::string_view text)
{
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return lower;
}

// the path under `folder` ("models/a/b.glb" under "models" is "a/b.glb"), or nothing when it isn't under it
std::optional<std::string> path_under(std::string_view path, std::string_view folder)
{
    if (path.size() <= folder.size() + 1 || !path.starts_with(folder) || path[folder.size()] != '/')
    {
        return std::nullopt;
    }
    return std::string(path.substr(folder.size() + 1));
}
}  // namespace

engine::AssetFileKind engine::get_asset_file_kind(std::string_view path, bool is_folder)
{
    if (is_folder)
    {
        return AssetFileKind::eFolder;
    }
    const auto extension = to_lower(std::filesystem::path(path).extension().string());
    if (extension == ".glb" || extension == ".gltf")
    {
        return AssetFileKind::eModel;
    }
    if (extension == ".json" && path_under(path, "scenes"))
    {
        return AssetFileKind::eScene;
    }
    if (extension == ".json" && path_under(path, "data"))
    {
        return AssetFileKind::eData;
    }
    constexpr std::array<std::string_view, 5> image_extensions = { ".png", ".jpg", ".jpeg", ".tga", ".bmp" };
    if (std::ranges::find(image_extensions, extension) != image_extensions.end())
    {
        return AssetFileKind::eTexture;
    }
    return AssetFileKind::eOther;
}

std::vector<engine::asset_entry_t> engine::list_asset_folder(const std::filesystem::path& assets_root, std::string_view folder)
{
    std::vector<asset_entry_t> entries;
    // never throws: the iterator is advanced with an error code (a range-for's ++ would throw), the listing stops at an
    // error
    std::error_code error;
    for (auto it = std::filesystem::directory_iterator(assets_root / folder, error); !error && it != std::filesystem::directory_iterator(); it.increment(error))
    {
        const auto& item = *it;
        const auto name = item.path().filename().generic_string();
        std::error_code type_error;
        const bool is_folder = item.is_directory(type_error);
        if (!is_folder && to_lower(item.path().extension().string()) == ".bin")
        {
            continue;
        }
        auto path = folder.empty() ? name : std::string(folder) + "/" + name;
        const auto kind = get_asset_file_kind(path, is_folder);
        entries.push_back({ name, std::move(path), kind });
    }
    std::ranges::sort(entries, [](const asset_entry_t& a, const asset_entry_t& b)
        {
            const bool a_folder = a.kind == AssetFileKind::eFolder;
            const bool b_folder = b.kind == AssetFileKind::eFolder;
            if (a_folder != b_folder)
            {
                return a_folder;
            }
            return to_lower(a.name) < to_lower(b.name);
        });
    return entries;
}

std::optional<std::string> engine::get_asset_id(const asset_entry_t& entry)
{
    switch (entry.kind)
    {
    case AssetFileKind::eModel:
    case AssetFileKind::eTexture: return path_under(entry.path, "models");
    case AssetFileKind::eScene: return path_under(entry.path, "scenes");
    case AssetFileKind::eData: return path_under(entry.path, "data");
    case AssetFileKind::eFolder:
    case AssetFileKind::eOther: break;
    }
    return std::nullopt;
}

std::vector<std::pair<std::uint32_t, std::string>> engine::get_model_assets(const std::vector<std::pair<std::uint32_t, std::string>>& assets, std::string_view model_id)
{
    const auto prefix = std::string(model_id) + "#";
    std::vector<std::pair<std::uint32_t, std::string>> model_assets;
    std::ranges::copy_if(assets, std::back_inserter(model_assets), [&prefix](const auto& asset) { return asset.second.starts_with(prefix); });
    return model_assets;
}

glm::vec3 engine::get_ground_point(const glm::vec3& ray_origin, const glm::vec3& ray_direction, float max_distance, float fallback_distance)
{
    // the ray reaches y = 0 in front of its origin, within max_distance (a ray just below the horizon meets it very far)
    if (ray_direction.y < -1e-4f && ray_origin.y > 0.0f)
    {
        if (const float distance = -ray_origin.y / ray_direction.y; distance <= max_distance)
        {
            return ray_origin + ray_direction * distance;
        }
    }
    return ray_origin + ray_direction * fallback_distance;
}
