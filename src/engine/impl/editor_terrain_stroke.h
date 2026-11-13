#pragma once
#include "editor_command_manager.h"
#include "terrain_brush.h"
#include "terrain_data.h"

#include <algorithm>
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
    // which heights of a patch
    enum class Side
    {
        eBefore,
        eAfter,
    };

    virtual ~ITerrainHeights() = default;
    // Gives the terrain (loaded from its file when it isn't loaded any more) the heights of the patches; false
    // (logged) when the terrain can't be found or a patch doesn't fit it (it was imported again with another size).
    virtual bool set_terrain_heights(std::string_view terrain, std::span<const TerrainPatch> patches, Side side) = 0;
};

// One stroke of a terrain brush as an undo step: the heights of the tiles the stroke touched, before and after
// (TerrainStroke records them). It changes a terrain asset, not the scene: the scene isn't unsaved by it (the terrain
// is, until it is saved), and like a data asset's steps it stays when the scene is released. GL-free, unit tested.
class CommandTerrainStroke : public ICommand
{
public:
    // `applied`: the terrain has the patches' heights after the stroke already (the brush set them), so the first
    // execute has nothing to set
    CommandTerrainStroke(ITerrainHeights& terrains, std::string_view terrain, std::vector<TerrainPatch> patches, bool applied = false)
        : terrains_(terrains)
        , terrain_(terrain)
        , patches_(std::move(patches))
        , applied_(applied)
    {
    }
    // one rectangle
    CommandTerrainStroke(ITerrainHeights& terrains, std::string_view terrain, const TerrainData::Region& region, std::vector<float> before,
        std::vector<float> after)
        : CommandTerrainStroke(terrains, terrain, one_patch(region, std::move(before), std::move(after)))
    {
    }

    void execute(Scene& /*sc*/) override
    {
        set_ = std::exchange(applied_, false) || terrains_.set_terrain_heights(terrain_, patches_, ITerrainHeights::Side::eAfter);
    }
    void undo(Scene& /*sc*/) override { terrains_.set_terrain_heights(terrain_, patches_, ITerrainHeights::Side::eBefore); }
    // a stroke which changed nothing (or whose terrain is gone) leaves no step
    bool has_effect() const override
    {
        return set_ && std::any_of(patches_.begin(), patches_.end(), [](const TerrainPatch& patch) { return patch.before != patch.after; });
    }
    bool changes_scene() const override { return false; }
    std::string_view get_terrain() const override { return terrain_; }

private:
    static std::vector<TerrainPatch> one_patch(const TerrainData::Region& region, std::vector<float> before, std::vector<float> after)
    {
        std::vector<TerrainPatch> patches;
        patches.push_back(TerrainPatch{ region, std::move(before), std::move(after) });
        return patches;
    }

    ITerrainHeights& terrains_;
    const std::string terrain_;
    const std::vector<TerrainPatch> patches_;
    // the heights after the stroke are in the terrain already
    bool applied_ = false;
    bool set_ = false;
};
}  // namespace engine
