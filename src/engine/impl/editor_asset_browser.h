#pragma once
#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine
{
// What a file in the assets folder is to the editor's asset browser (like Unity's Project window or Godot's FileSystem
// dock), told by its extension and, for JSON, by where it is.
enum class AssetFileKind
{
    eFolder,
    eModel,    // .glb / .gltf anywhere: a prefab named by its path under assets/models
    eScene,    // .json under assets/scenes: a scene file, placed as a scene prefab named by its path there
    eTexture,  // an image file (.png, .jpg, ...)
    eData,     // .json under assets/data: a data asset (DataAsset) named by its path there
    eOther,    // shaders, fonts, UI documents, ...: listed, but nothing uses them from the browser
};

// a folder or file under the assets folder
struct asset_entry_t
{
    std::string name;
    // relative to the assets folder, '/' separated ("models/characters/Rogue.glb")
    std::string path;
    AssetFileKind kind = AssetFileKind::eOther;
};

// `path` relative to the assets folder, '/' separated
AssetFileKind get_asset_file_kind(std::string_view path, bool is_folder);

// The file selected in the editor's asset browser (AssetBrowserPanel), by its path under assets/ ("" for none), for the
// selection commands (CommandSetSelection): the editor selects one thing, a game object or a file, so selecting one
// deselects the other in the same undo step.
class IAssetSelection
{
public:
    virtual ~IAssetSelection() = default;
    virtual std::string get_selected_asset() const = 0;
    virtual void set_selected_asset(const std::string& path) = 0;
};

// The folders and files directly in `folder` (relative to `assets_root`, "" for the root itself): the folders first,
// then the files, each by name ignoring case. glTF buffers (.bin) are left out: they are part of their .gltf model.
// Nothing when the folder doesn't exist.
std::vector<asset_entry_t> list_asset_folder(const std::filesystem::path& assets_root, std::string_view folder);

// The asset ID the engine names the file by, or nothing when it has none: a model or an image by its path under
// models/ (images in their own files are named like models, so models using them share one texture), a scene file by
// its path under scenes/, a data asset by its path under data/.
std::optional<std::string> get_asset_id(const asset_entry_t& entry);

// The scene file Save As writes for the name typed into its dialog in `folder` (relative to the assets folder,
// "scenes" or one under it), as its asset ID (its path under assets/scenes, "rooms/room_a.json"), ".json" added when
// the name has no extension. Nothing for a name which isn't one file name (empty, ".", "..", a path, other characters
// file systems refuse), has another extension than .json, or a folder outside assets/scenes: like Unreal's Save As, a
// scene is saved inside the content folder, where it has an asset ID and can be placed as a prefab.
std::optional<std::string> get_scene_save_path(std::string_view folder, std::string_view name);

// The assets created from a model (asset IDs "<model ID>#<name>", see asset_id.h) among `assets` (an atlas' handles and
// names), in their order.
std::vector<std::pair<std::uint32_t, std::string>> get_model_assets(const std::vector<std::pair<std::uint32_t, std::string>>& assets, std::string_view model_id);

// Drag and drop payloads of the asset browser (ImGui payload types, at most 32 characters), each holding an asset ID
// as a zero-terminated string. A drop target accepts only the type it can use, so a texture can't be dropped on a mesh
// field.
inline constexpr const char* ASSET_PAYLOAD_PREFAB = "ASSET_PREFAB";      // a model or a scene file, placed in the scene
inline constexpr const char* ASSET_PAYLOAD_GEOMETRY = "ASSET_GEOMETRY";  // a mesh of a model, "<model path>#<name>"
inline constexpr const char* ASSET_PAYLOAD_TEXTURE = "ASSET_TEXTURE";    // an image file, or an image of a model

// Where an asset dropped along the ray lands when it hits no collider: on the ground plane (y = 0) when the ray goes
// down to it within `max_distance` (the camera's far plane: farther, it couldn't be seen), otherwise `fallback_distance`
// along the ray (like Unity, which places it in front of the camera then).
glm::vec3 get_ground_point(const glm::vec3& ray_origin, const glm::vec3& ray_direction, float max_distance, float fallback_distance = 10.0f);
}  // namespace engine
