#pragma once
#include "editor_command_manager.h"
#include "terrain_data.h"

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine
{
// The terrains of the application, for the editor's commands: by name, so a step finds its terrain again after it was
// released and loaded since.
class ITerrainHeights
{
public:
    virtual ~ITerrainHeights() = default;
    // Gives the terrain (loaded from its file when it isn't loaded any more) the heights of the region; false (logged)
    // when the terrain can't be found or the region doesn't fit it (it was imported again with another size).
    virtual bool set_terrain_heights(std::string_view terrain, const TerrainData::Region& region, std::span<const float> heights) = 0;
};

// One stroke of a terrain brush as an undo step: the heights of the rectangle the stroke touched, before and after
// (TerrainStroke records them). It changes a terrain asset, not the scene: the scene isn't unsaved by it (the terrain
// is, until it is saved), and like a data asset's steps it stays when the scene is released. GL-free, unit tested.
class CommandTerrainStroke : public ICommand
{
public:
    CommandTerrainStroke(ITerrainHeights& terrains, std::string_view terrain, const TerrainData::Region& region, std::vector<float> before,
        std::vector<float> after)
        : terrains_(terrains)
        , terrain_(terrain)
        , region_(region)
        , before_(std::move(before))
        , after_(std::move(after))
    {
    }

    void execute(Scene& /*sc*/) override { set_ = terrains_.set_terrain_heights(terrain_, region_, after_); }
    void undo(Scene& /*sc*/) override { terrains_.set_terrain_heights(terrain_, region_, before_); }
    // a stroke which changed nothing (or whose terrain is gone) leaves no step
    bool has_effect() const override { return set_ && before_ != after_; }
    bool changes_scene() const override { return false; }
    bool changes_asset() const override { return true; }

private:
    ITerrainHeights& terrains_;
    const std::string terrain_;
    const TerrainData::Region region_;
    const std::vector<float> before_;
    const std::vector<float> after_;
    bool set_ = false;
};
}  // namespace engine
