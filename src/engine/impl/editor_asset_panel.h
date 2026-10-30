#pragma once
#include "editor_asset_browser.h"

#include <engine.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine
{
class Application;

// The editor's asset browser, like Unity's Project window and Godot's FileSystem dock: the folders of assets/ as a
// tree, the folders and files of the open one with an icon per kind (double-click a folder to open it), and the
// details of the selected file for the properties panel. Models and scene files are dragged into the scene view to
// place them (ASSET_PAYLOAD_PREFAB); images, and the meshes and images of a model (listed under it when it is opened
// with its arrow, like Unity's sub-assets, and in its details), onto the asset fields of the properties panel
// (ASSET_PAYLOAD_GEOMETRY, ASSET_PAYLOAD_TEXTURE). Dragging doesn't select, so a file can be dropped on the selected
// game object's fields.
// Selecting a model or an image, or opening a model, loads it to show what it holds, and holds it until the selection
// changes or the model is closed. The folders are listed once and again after Refresh.
// The selection is the editor's (IAssetSelection): a click asks for it, and the editor selects the file with an undo
// step which deselects the game object.
class AssetBrowserPanel : public IAssetSelection
{
public:
    // `app` must outlive the panel
    explicit AssetBrowserPanel(Application& app);
    AssetBrowserPanel(const AssetBrowserPanel&) = delete;
    AssetBrowserPanel& operator=(const AssetBrowserPanel&) = delete;
    ~AssetBrowserPanel() override;

    // The panel's contents, in the current ImGui window. The path of the file clicked this frame when it isn't the
    // selected one, for the editor to select.
    std::optional<std::string> draw();
    // the selected file's details, in the current ImGui window
    void draw_inspector();

    bool has_selection() const { return selected_.has_value(); }
    std::string get_selected_asset() const override;
    // selects the file (a path under assets/), or nothing for ""
    void set_selected_asset(const std::string& path) override;

private:
    void clear_selection();
    // what a loaded model holds, listed once when it is loaded: its assets live as long as the panel holds the prefab
    struct model_contents_t
    {
        std::vector<std::pair<std::uint32_t, std::string>> meshes;
        std::vector<std::pair<std::uint32_t, std::string>> images;
        std::string skin;  // empty when it has none
        std::vector<std::string> animations;  // by name
    };
    struct opened_model_t
    {
        // ENGINE_INVALID_OBJECT_HANDLE when it couldn't be loaded
        std::uint32_t prefab = ENGINE_INVALID_OBJECT_HANDLE;
        model_contents_t contents;
    };
    model_contents_t get_model_contents(const std::string& model_id) const;
    // loads the model to show what it holds (logs why when it can't)
    opened_model_t open_model(const std::string& model_id);
    void close_model(opened_model_t& model);

    // lists the folders again; opened models, the selection and the open folder whose files are gone are dropped
    void refresh();
    // the folder's entries, listed when first asked for
    const std::vector<asset_entry_t>& get_folder(const std::string& folder);
    void draw_folder_tree(const std::string& folder, std::string_view name);
    void draw_path_bar();
    // the path of the file clicked when it isn't the selected one
    std::optional<std::string> draw_files();
    void select(const asset_entry_t& entry);
    void draw_model_details();
    void draw_texture_details();

private:
    Application& app_;
    std::filesystem::path assets_root_;
    // relative to the assets folder, "" for the root
    std::string current_folder_;
    // the tree opens the current folder's ancestors once after it changed from elsewhere (the file list, the path bar)
    bool reveal_current_folder_ = false;
    std::map<std::string, std::vector<asset_entry_t>> folders_;
    std::optional<asset_entry_t> selected_;
    // the references the selection holds, to show its details
    opened_model_t inspected_model_;
    std::uint32_t inspected_texture_ = ENGINE_INVALID_OBJECT_HANDLE;
    // the opened models by path, each held while open
    std::map<std::string, opened_model_t> expanded_models_;
};
}  // namespace engine
