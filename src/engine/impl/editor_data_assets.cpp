#include "editor_data_assets.h"
#include "application.h"
#include "data_asset.h"
#include "logger.h"

#include <fmt/format.h>

engine::EditedDataAssets::~EditedDataAssets()
{
    for (const auto& [asset_id, handle] : held_)
    {
        app_.release_data_asset(handle);
    }
}

bool engine::EditedDataAssets::set_data_asset_value_text(std::string_view asset_id, std::string_view text)
{
    // the one loaded, or loaded again from its file when it was released since (undo of an edit saved meanwhile)
    const auto handle = app_.load_data_asset(asset_id);
    if (handle == ENGINE_INVALID_OBJECT_HANDLE)
    {
        log::log(log::LogLevel::eError, fmt::format("Editor: the data asset {} can't be loaded, its value isn't changed.\n", asset_id));
        return false;
    }
    const bool set = app_.set_data_asset_value_text(handle, text);
    hold_while_dirty(std::string(asset_id), handle);
    app_.release_data_asset(handle);
    return set;
}

bool engine::EditedDataAssets::save(std::string_view asset_id)
{
    const auto it = held_.find(asset_id);
    if (it == held_.end())
    {
        return true;
    }
    const auto handle = it->second;
    const bool saved = app_.save_data_asset_file(handle);
    if (saved)
    {
        log::log(log::LogLevel::eTrace, fmt::format("Data asset {} saved.\n", asset_id));
    }
    hold_while_dirty(std::string(asset_id), handle);
    return saved;
}

bool engine::EditedDataAssets::save_all()
{
    bool saved = true;
    for (const auto& asset_id : get_unsaved())
    {
        saved &= save(asset_id);
    }
    return saved;
}

void engine::EditedDataAssets::on_reloaded(std::string_view asset_id)
{
    if (const auto it = held_.find(asset_id); it != held_.end())
    {
        hold_while_dirty(it->first, it->second);
    }
}

std::vector<std::string> engine::EditedDataAssets::get_unsaved() const
{
    std::vector<std::string> unsaved;
    for (const auto& [asset_id, handle] : held_)
    {
        unsaved.push_back(asset_id);
    }
    return unsaved;
}

void engine::EditedDataAssets::hold_while_dirty(const std::string& asset_id, std::uint32_t handle)
{
    const auto* asset = app_.get_data_asset(handle);
    const bool dirty = asset && asset->is_dirty();
    const auto it = held_.find(asset_id);
    if (dirty && it == held_.end())
    {
        // one more reference to the loaded asset
        held_.emplace(asset_id, app_.load_data_asset(asset_id));
    }
    else if (!dirty && it != held_.end())
    {
        const auto held = it->second;
        held_.erase(it);
        app_.release_data_asset(held);
    }
}
