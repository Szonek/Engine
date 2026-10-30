#pragma once
#include "serialization/scene_serializer.h"
#include "serialization/type_registry.h"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace engine
{
// A data asset (like Unity's ScriptableObject, Unreal's DataAsset, Godot's Resource): one value of a game-defined struct
// type, read from its file under assets/data (see load_data_asset for the format), named by its path there. It holds the
// assets its value references (through the resolver it read the file with) as long as it has the value. While the editor
// watches files (FileWatcher), a change of the file marks it, and the application reads it again at the start of the
// next frame (reload_if_changed, on the main thread: the watcher calls back on its own thread), into the same asset.
class DataAsset
{
public:
    // a new resolver for each read of the file: it holds the assets the value read with it references
    using AssetsFactory = std::function<std::unique_ptr<IAssetResolver>()>;

    // nullptr (logged) when the file can't be read or isn't a data asset file
    static std::unique_ptr<DataAsset> load(std::string_view asset_id, const std::filesystem::path& file_path, const TypeRegistry& types, AssetsFactory make_assets);
    DataAsset(const DataAsset&) = delete;
    DataAsset& operator=(const DataAsset&) = delete;
    ~DataAsset();

    const std::string& get_name() const { return name_; }
    const TypeInfo& get_type() const { return *type_; }
    const std::byte* get_value() const { return value_->get(); }

    // the file changed; any thread
    void mark_changed() { changed_ = true; }
    // Reads the file again when it changed. The value changes only when the file is a data asset of the same type
    // (otherwise logged, the old value stays, and it is read again on the next change). Returns whether it changed.
    bool reload_if_changed();

private:
    DataAsset(std::string_view asset_id, const std::filesystem::path& file_path, const TypeRegistry& types, AssetsFactory make_assets);
    // the file's value and the resolver holding its assets, or nothing (logged)
    bool read(DataAssetValue& value, std::unique_ptr<IAssetResolver>& assets) const;

    std::string name_;
    std::filesystem::path file_path_;
    const TypeRegistry& types_;
    AssetsFactory make_assets_;
    const TypeInfo* type_ = nullptr;
    std::unique_ptr<ValueBuffer> value_;
    std::unique_ptr<IAssetResolver> assets_;
    std::atomic<bool> changed_ = false;
};
}  // namespace engine
