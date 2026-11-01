#include "data_asset.h"
#include "file_io.h"
#include "file_watcher.h"
#include "logger.h"

#include <fmt/format.h>

#include <utility>

namespace
{
// the resolver a data asset reads its file with, noting which data assets the value references
class RecordingAssets : public engine::IAssetResolver
{
public:
    RecordingAssets(engine::IAssetResolver& assets, std::vector<std::uint32_t>& data_assets)
        : assets_(assets)
        , data_assets_(data_assets)
    {
    }

    std::string get_asset_id(engine::AssetKind kind, std::uint32_t handle) const override { return assets_.get_asset_id(kind, handle); }
    std::uint32_t resolve(engine::AssetKind kind, std::string_view asset_id) override
    {
        const auto handle = assets_.resolve(kind, asset_id);
        if (kind == engine::AssetKind::eDataAsset && handle != ENGINE_INVALID_OBJECT_HANDLE)
        {
            data_assets_.push_back(handle);
        }
        return handle;
    }
    const engine::TypeInfo* get_data_asset_type(std::uint32_t handle) const override { return assets_.get_data_asset_type(handle); }

private:
    engine::IAssetResolver& assets_;
    std::vector<std::uint32_t>& data_assets_;
};
}  // namespace

std::unique_ptr<engine::DataAsset> engine::DataAsset::load(std::string_view asset_id, const std::filesystem::path& file_path, const TypeRegistry& types,
    AssetsFactory make_assets, std::function<void()> on_changed)
{
    std::unique_ptr<DataAsset> asset(new DataAsset(asset_id, file_path, types, std::move(make_assets)));
    auto contents = read_text_file(file_path, fmt::format("Data asset {}", asset_id));
    DataAssetValue value;
    std::unique_ptr<IAssetResolver> assets;
    std::vector<std::uint32_t> data_assets;
    if (!contents || !asset->parse(*contents, value, assets, data_assets))
    {
        return nullptr;
    }
    asset->type_ = value.type;
    asset->take(std::move(value), std::move(assets), std::move(data_assets));
    asset->file_value_text_ = asset->value_text_;
    asset->file_contents_ = std::move(*contents);
    asset->watched_ = FileWatcher::get_instance().register_callback(file_path, [a = asset.get(), on_changed = std::move(on_changed)] {
        a->mark_changed();
        if (on_changed)
        {
            on_changed();
        }
    });
    return asset;
}

engine::DataAsset::DataAsset(std::string_view asset_id, const std::filesystem::path& file_path, const TypeRegistry& types, AssetsFactory make_assets)
    : name_(asset_id)
    , file_path_(file_path)
    , types_(types)
    , make_assets_(std::move(make_assets))
{
}

engine::DataAsset::~DataAsset()
{
    if (watched_)
    {
        FileWatcher::get_instance().unregister_callback(file_path_);
    }
}

engine::DataAsset::ReloadResult engine::DataAsset::reload_if_changed()
{
    if (!changed_.exchange(false))
    {
        return ReloadResult::eNone;
    }
    auto contents = read_text_file(file_path_, fmt::format("Data asset {}", name_));
    // what was last read or written (the change event of a save): nothing new
    if (contents && *contents == file_contents_)
    {
        return ReloadResult::eNone;
    }
    DataAssetValue value;
    std::unique_ptr<IAssetResolver> assets;
    std::vector<std::uint32_t> data_assets;
    if (!contents || !parse(*contents, value, assets, data_assets))
    {
        log::log(log::LogLevel::eError, fmt::format("Data asset {}: it keeps its value until its file can be read again.\n", name_));
        return ReloadResult::eNone;
    }
    if (value.type != type_)
    {
        // the handle is referenced as a value of its type (checked when a scene file is loaded)
        log::log(log::LogLevel::eError, fmt::format("Data asset {}: its file now holds a {} instead of a {}, it keeps its value.\n", name_, value.type->name, type_->name));
        return ReloadResult::eNone;
    }
    file_contents_ = std::move(*contents);
    if (is_dirty())
    {
        // the file's value is what it is dirty against now
        file_value_text_ = save_data_asset(*value.type, value.value->get(), *assets);
        log::log(log::LogLevel::eError, fmt::format("Data asset {}: its file changed while it has unsaved changes in the editor; it keeps the editor's value, "
            "and saving it writes that over the file.\n", name_));
        return ReloadResult::eKeptValue;
    }
    take(std::move(value), std::move(assets), std::move(data_assets));
    file_value_text_ = value_text_;
    reload_count_++;
    log::log(log::LogLevel::eTrace, fmt::format("Data asset {} reloaded.\n", name_));
    return ReloadResult::eReloaded;
}

bool engine::DataAsset::set_value_text(std::string_view text)
{
    DataAssetValue value;
    std::unique_ptr<IAssetResolver> assets;
    std::vector<std::uint32_t> data_assets;
    if (!parse(text, value, assets, data_assets))
    {
        log::log(log::LogLevel::eError, fmt::format("Data asset {}: the new value can't be used, it keeps its value.\n", name_));
        return false;
    }
    if (value.type != type_)
    {
        log::log(log::LogLevel::eError, fmt::format("Data asset {}: the new value is a {} instead of a {}, it keeps its value.\n", name_, value.type->name, type_->name));
        return false;
    }
    take(std::move(value), std::move(assets), std::move(data_assets));
    reload_count_++;
    return true;
}

bool engine::DataAsset::save()
{
    if (!write_file_atomic(file_path_, value_text_, fmt::format("Data asset {}", name_)))
    {
        return false;
    }
    file_value_text_ = value_text_;
    file_contents_ = value_text_;
    return true;
}

bool engine::DataAsset::parse(std::string_view text, DataAssetValue& value, std::unique_ptr<IAssetResolver>& assets, std::vector<std::uint32_t>& data_assets) const
{
    assets = make_assets_();
    RecordingAssets recording(*assets, data_assets);
    // logs why it can't be used
    auto loaded = load_data_asset(text, name_, types_, recording);
    if (!loaded)
    {
        return false;
    }
    value = std::move(*loaded);
    return true;
}

void engine::DataAsset::take(DataAssetValue&& value, std::unique_ptr<IAssetResolver>&& assets, std::vector<std::uint32_t>&& data_assets)
{
    value_ = std::move(value.value);
    // the old value's assets are released after the new value's are held, so the ones both use stay loaded
    assets_ = std::move(assets);
    data_assets_ = std::move(data_assets);
    value_text_ = save_data_asset(*type_, value_->get(), *assets_);
}
