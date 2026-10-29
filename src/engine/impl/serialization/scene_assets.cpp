#include "scene_assets.h"
#include "../application.h"
#include "../asset_store.h"

#include <filesystem>

engine::SceneAssets::SceneAssets(Application& app)
    : app_(app)
{
}

engine::SceneAssets::~SceneAssets()
{
    for (const auto& [file, prefab] : prefabs_)
    {
        if (prefab != ENGINE_INVALID_OBJECT_HANDLE)
        {
            app_.release_prefab(prefab);
        }
    }
    for (const auto& [file, texture] : image_files_)
    {
        if (texture != ENGINE_INVALID_OBJECT_HANDLE)
        {
            app_.release_texture(texture);
        }
    }
}

std::string engine::SceneAssets::get_asset_id(AssetKind kind, std::uint32_t handle) const
{
    switch (kind)
    {
    case AssetKind::eGeometry: return app_.get_geometry_name(handle);
    case AssetKind::eTexture: return app_.get_texture_name(handle);
    case AssetKind::eSkin: return app_.get_skin_name(handle);
    case AssetKind::eAnimationSet: return app_.get_animation_set_name(handle);
    }
    return {};
}

std::uint32_t engine::SceneAssets::resolve(AssetKind kind, std::string_view asset_id)
{
    hold_source_file(kind, asset_id);
    switch (kind)
    {
    case AssetKind::eGeometry: return app_.get_geometry(asset_id);
    case AssetKind::eTexture: return app_.get_texture(asset_id);
    case AssetKind::eSkin: return app_.get_skin(asset_id);
    case AssetKind::eAnimationSet: return app_.get_animation_set(asset_id);
    }
    return ENGINE_INVALID_OBJECT_HANDLE;
}

std::uint32_t engine::SceneAssets::resolve_prefab(std::string_view model_path)
{
    auto it = prefabs_.find(model_path);
    if (it == prefabs_.end())
    {
        // logs why when it fails
        it = prefabs_.emplace(model_path, app_.load_prefab(model_path)).first;
    }
    return it->second;
}

void engine::SceneAssets::hold_source_file(AssetKind kind, std::string_view asset_id)
{
    // an asset of a model: "<model path>#<name>"
    if (const auto separator = asset_id.find('#'); separator != std::string_view::npos)
    {
        resolve_prefab(asset_id.substr(0, separator));
        return;
    }
    // an image in its own file, named by its path under assets/models
    if (kind == AssetKind::eTexture && !image_files_.contains(asset_id)
        && std::filesystem::is_regular_file(AssetStore::get_instance().get_models_base_path() / asset_id))
    {
        image_files_.emplace(asset_id, app_.load_texture_file(asset_id));
    }
}
