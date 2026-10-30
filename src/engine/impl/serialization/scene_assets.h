#pragma once
#include "scene_serializer.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace engine
{
class Application;

// The assets a scene loaded from a file uses. An asset is kept alive by its file: the scene holds one reference to
// each file it references (a model's prefab for "<model path>#<name>", an image file named by its path under
// assets/models, a data asset named by its path under assets/data) whether the file was loaded already or is loaded
// now, and releases them when it is destroyed. A data asset holds the assets its value references the same way. So the
// scene's assets live as long as the scene, also when the game releases the same prefab. Assets which don't come from
// a file (the default texture, geometries the game creates) are found by name and held by whoever created them.
class SceneAssets : public IAssetResolver
{
public:
    explicit SceneAssets(Application& app);
    SceneAssets(const SceneAssets&) = delete;
    SceneAssets& operator=(const SceneAssets&) = delete;
    ~SceneAssets() override;

    std::string get_asset_id(AssetKind kind, std::uint32_t handle) const override;
    std::uint32_t resolve(AssetKind kind, std::string_view asset_id) override;
    const TypeInfo* get_data_asset_type(std::uint32_t handle) const override;
    // the prefab of a model (its asset ID is the model's path), loaded when needed; ENGINE_INVALID_OBJECT_HANDLE when
    // it can't be loaded (logged)
    std::uint32_t resolve_prefab(std::string_view model_path);

private:
    // holds a reference to the file the asset comes from, loading it when needed
    void hold_source_file(AssetKind kind, std::string_view asset_id);

    Application& app_;
    // file -> the handle the scene holds a reference to, or ENGINE_INVALID_OBJECT_HANDLE when it failed loading (it
    // isn't tried again)
    std::map<std::string, std::uint32_t, std::less<>> prefabs_;
    std::map<std::string, std::uint32_t, std::less<>> image_files_;
    std::map<std::string, std::uint32_t, std::less<>> data_assets_;
};
}  // namespace engine
