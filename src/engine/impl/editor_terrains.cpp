#include "editor_terrains.h"
#include "application.h"
#include "logger.h"

#include <fmt/format.h>

#include <filesystem>

engine::EditedTerrains::~EditedTerrains()
{
    for (const auto& [terrain, handle] : held_)
    {
        app_.release_terrain(handle);
    }
}

bool engine::EditedTerrains::is_file(std::string_view terrain)
{
    return std::filesystem::path(terrain).extension() == ".terrain";
}

bool engine::EditedTerrains::set_terrain_heights(std::string_view terrain, std::span<const TerrainPatch> patches, Side side)
{
    // the one loaded, or loaded again from its file when it was released since (undo of a stroke saved meanwhile)
    auto handle = app_.get_terrain(terrain);
    const bool loaded_here = handle == ENGINE_INVALID_OBJECT_HANDLE && is_file(terrain);
    if (loaded_here)
    {
        handle = app_.load_terrain(terrain);
    }
    auto* asset = app_.get_terrain(handle);
    if (!asset)
    {
        log::log(log::LogLevel::eError, fmt::format("Editor: the terrain {} can't be found, its heights aren't changed.\n", terrain));
        return false;
    }
    // logs why when the region doesn't fit
    bool set = true;
    {
        // one change for the collision
        const auto batch = asset->begin_edit_batch();
        for (const auto& patch : patches)
        {
            set &= asset->set_heights(patch.region, side == Side::eBefore ? patch.before : patch.after);
        }
    }
    // an undo back to the heights which were saved makes it clean again
    asset->refresh_dirty();
    hold_while_dirty(std::string(terrain), handle);
    if (loaded_here)
    {
        app_.release_terrain(handle);
    }
    return set;
}

void engine::EditedTerrains::on_edited(std::string_view terrain)
{
    if (const auto handle = app_.get_terrain(terrain); handle != ENGINE_INVALID_OBJECT_HANDLE)
    {
        hold_while_dirty(std::string(terrain), handle);
    }
}

bool engine::EditedTerrains::save(std::string_view terrain)
{
    const auto it = held_.find(terrain);
    if (it == held_.end())
    {
        return true;
    }
    const auto handle = it->second;
    const bool saved = app_.save_terrain(handle);
    if (saved)
    {
        log::log(log::LogLevel::eTrace, fmt::format("Terrain {} saved.\n", terrain));
    }
    hold_while_dirty(std::string(terrain), handle);
    return saved;
}

bool engine::EditedTerrains::save_all()
{
    bool saved = true;
    for (const auto& terrain : get_unsaved())
    {
        saved &= save(terrain);
    }
    return saved;
}

std::vector<std::string> engine::EditedTerrains::get_unsaved() const
{
    std::vector<std::string> unsaved;
    for (const auto& [terrain, handle] : held_)
    {
        unsaved.push_back(terrain);
    }
    return unsaved;
}

void engine::EditedTerrains::hold_while_dirty(const std::string& terrain, std::uint32_t handle)
{
    const auto* asset = app_.get_terrain(handle);
    const bool dirty = asset && asset->is_dirty() && is_file(terrain);
    const auto it = held_.find(terrain);
    if (dirty && it == held_.end())
    {
        // one more reference to the loaded terrain
        if (app_.acquire_terrain(handle))
        {
            held_.emplace(terrain, handle);
        }
    }
    else if (!dirty && it != held_.end())
    {
        const auto held = it->second;
        held_.erase(it);
        app_.release_terrain(held);
    }
}
