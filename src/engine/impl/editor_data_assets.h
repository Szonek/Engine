#pragma once
#include "editor_commands.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace engine
{
class Application;

// The data assets the editor edits (CommandUpdateDataAsset), like dirty assets in Unity and Unreal: every one with
// unsaved changes is held until it is saved or its changes are undone, so it isn't released with the last scene or
// selection referencing it, and Save All (Ctrl+S) or the unsaved changes prompt can still write it.
class EditedDataAssets : public IDataAssetValues
{
public:
    // `app` must outlive it
    explicit EditedDataAssets(Application& app) : app_(app) {}
    EditedDataAssets(const EditedDataAssets&) = delete;
    EditedDataAssets& operator=(const EditedDataAssets&) = delete;
    // the unsaved changes are lost (closing the window asks first)
    ~EditedDataAssets() override;

    bool set_data_asset_value_text(std::string_view asset_id, std::string_view text) override;

    // writes the data asset's value to its file, when it has unsaved changes; false (logged) when it can't be written
    bool save(std::string_view asset_id);
    // every data asset with unsaved changes; false when one can't be written (the others are)
    bool save_all();
    bool has_unsaved() const { return !held_.empty(); }
    bool is_unsaved(std::string_view asset_id) const { return held_.contains(asset_id); }
    // the data asset's file was read again: it is dirty against the file's new value, or clean (the file now holds
    // what the editor gave it), and held while it is dirty
    void on_reloaded(std::string_view asset_id);
    // the asset IDs of the data assets with unsaved changes
    std::vector<std::string> get_unsaved() const;

private:
    // holds the data asset while it is dirty, releases it once it isn't
    void hold_while_dirty(const std::string& asset_id, std::uint32_t handle);

    Application& app_;
    // the dirty data assets by asset ID, each with a reference of its own
    std::map<std::string, std::uint32_t, std::less<>> held_;
};
}  // namespace engine
