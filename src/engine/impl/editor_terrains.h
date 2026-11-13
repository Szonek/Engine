#pragma once
#include "editor_terrain_stroke.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace engine
{
class Application;

// The terrains the editor sculpts, like EditedDataAssets: every one with unsaved changes is held until it is saved or
// its changes are undone, so it isn't released with the last scene referencing it, and Save All (Ctrl+S) or the
// unsaved changes prompt can still write it. Only terrains which are files count (their name is a .terrain path): one
// made by code can be sculpted, but there is nothing to save it to.
class EditedTerrains : public ITerrainHeights
{
public:
    // `app` must outlive it
    explicit EditedTerrains(Application& app) : app_(app) {}
    EditedTerrains(const EditedTerrains&) = delete;
    EditedTerrains& operator=(const EditedTerrains&) = delete;
    // the unsaved changes are lost (closing the window asks first)
    ~EditedTerrains() override;

    bool set_terrain_heights(std::string_view terrain, std::span<const TerrainPatch> patches, Side side) override;
    // the terrain was changed directly (a brush during a stroke), saved or its heights replaced by someone else: held
    // while it is dirty
    void on_edited(std::string_view terrain);

    // whether the terrain is a file the editor can write
    static bool is_file(std::string_view terrain);
    // writes the terrain to its file, when it has unsaved changes; false (logged) when it can't be written
    bool save(std::string_view terrain);
    // every terrain with unsaved changes; false when one can't be written (the others are)
    bool save_all();
    bool has_unsaved() const { return !held_.empty(); }
    bool is_unsaved(std::string_view terrain) const { return held_.contains(terrain); }
    // the names of the terrains with unsaved changes
    std::vector<std::string> get_unsaved() const;

private:
    // holds the terrain while it is dirty, releases it once it isn't
    void hold_while_dirty(const std::string& terrain, std::uint32_t handle);

    Application& app_;
    // the dirty terrains by name, each with a reference of its own
    std::map<std::string, std::uint32_t, std::less<>> held_;
};
}  // namespace engine
