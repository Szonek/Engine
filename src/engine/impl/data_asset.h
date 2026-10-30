#pragma once
#include "serialization/scene_serializer.h"
#include "serialization/type_registry.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace engine
{
// A data asset (like Unity's ScriptableObject, Unreal's DataAsset, Godot's Resource): one value of a game-defined struct
// type, read from its file under assets/data (see load_data_asset for the format), named by its path there. It holds the
// assets its value references (through the resolver it read the file with) as long as it has the value. While the editor
// watches files (FileWatcher), a change of the file marks it, and the application reads it again at the start of the
// next frame (reload_if_changed, on the main thread: the watcher calls back on its own thread), into the same asset.
// A file is watched by the first asset loading it: another asset ID naming the same file (an absolute path) isn't
// reloaded.
class DataAsset
{
public:
    // a new resolver for each read of the file: it holds the assets the value read with it references
    using AssetsFactory = std::function<std::unique_ptr<IAssetResolver>()>;

    // nullptr (logged) when the file can't be read or isn't a data asset file. `on_changed` is called (on the watcher's
    // thread) when the file changed, after the asset is marked.
    static std::unique_ptr<DataAsset> load(std::string_view asset_id, const std::filesystem::path& file_path, const TypeRegistry& types, AssetsFactory make_assets,
        std::function<void()> on_changed = {});
    DataAsset(const DataAsset&) = delete;
    DataAsset& operator=(const DataAsset&) = delete;
    ~DataAsset();

    const std::string& get_name() const { return name_; }
    const TypeInfo& get_type() const { return *type_; }
    const std::byte* get_value() const { return value_->get(); }
    // the data assets the resolver holding the value's assets holds (the ones the file references, as it found them)
    const std::vector<std::uint32_t>& get_data_assets() const { return data_assets_; }

    // how many times the value was read again, so a view of it knows when to refresh
    std::uint64_t get_reload_count() const { return reload_count_; }

    // the file changed; any thread
    void mark_changed() { changed_ = true; }
    // Reads the file again when it changed. The value changes only when the file is a data asset of the same type
    // (otherwise logged, the old value stays, and it is read again on the next change). Returns whether it changed.
    bool reload_if_changed();

private:
    DataAsset(std::string_view asset_id, const std::filesystem::path& file_path, const TypeRegistry& types, AssetsFactory make_assets);
    // the file's value, the resolver holding its assets and the data assets it references, or nothing (logged)
    bool read(DataAssetValue& value, std::unique_ptr<IAssetResolver>& assets, std::vector<std::uint32_t>& data_assets) const;

    std::string name_;
    std::filesystem::path file_path_;
    const TypeRegistry& types_;
    AssetsFactory make_assets_;
    const TypeInfo* type_ = nullptr;
    std::unique_ptr<ValueBuffer> value_;
    std::unique_ptr<IAssetResolver> assets_;
    std::vector<std::uint32_t> data_assets_;
    std::uint64_t reload_count_ = 0;
    std::atomic<bool> changed_ = false;
    // whether this asset registered its file with the watcher (so it unregisters it)
    bool watched_ = false;
};
}  // namespace engine
