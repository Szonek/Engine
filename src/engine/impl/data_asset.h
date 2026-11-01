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
// reloaded. The editor gives it new values (set_value_text) and writes them to its file (save), like an asset edited
// in Unity's inspector: it is dirty while its value differs from its file's.
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
    // the value as save_data_asset writes it (asset references by asset ID)
    const std::string& get_value_text() const { return value_text_; }
    // the data assets the resolver holding the value's assets holds (the ones the file references, as it found them)
    const std::vector<std::uint32_t>& get_data_assets() const { return data_assets_; }

    // how many times the value changed (read again or set), so a view of it knows when to refresh
    std::uint64_t get_reload_count() const { return reload_count_; }

    // the file changed; any thread
    void mark_changed() { changed_ = true; }
    enum class ReloadResult
    {
        eNone,       // nothing new: no change, the file's own save, or a file which can't be used (logged)
        eReloaded,   // the value is the file's new one
        eKeptValue,  // the file changed while the asset is dirty: it keeps its value (logged), dirty against the file's new one
    };
    // Reads the file again when it changed. The value changes only when the file is a data asset of the same type
    // (otherwise logged, the old value stays, and it is read again on the next change) and holds something else than
    // what was last read from it or written to it (so a save doesn't read the file again). A dirty asset keeps its
    // value (logged): the editor's edits win, and the next save writes them over the file.
    ReloadResult reload_if_changed();

    // A new value, as a data asset file of the same type (save_data_asset's text: asset references by asset ID). Its
    // assets are held through a new resolver, like a reload, and the old value's released after them. False (logged)
    // when the text can't be used or holds another type; the value stays then.
    bool set_value_text(std::string_view text);
    // whether the value differs from what its file holds
    bool is_dirty() const { return value_text_ != file_value_text_; }
    // Writes the value to its file, atomically (write_file_atomic). False (logged) when it can't be written; it stays
    // dirty then.
    bool save();

private:
    DataAsset(std::string_view asset_id, const std::filesystem::path& file_path, const TypeRegistry& types, AssetsFactory make_assets);
    // the value of a data asset file's text, the resolver holding its assets and the data assets it references, or
    // nothing (logged)
    bool parse(std::string_view text, DataAssetValue& value, std::unique_ptr<IAssetResolver>& assets, std::vector<std::uint32_t>& data_assets) const;
    // takes the parsed value, its resolver and data assets, and writes its text
    void take(DataAssetValue&& value, std::unique_ptr<IAssetResolver>&& assets, std::vector<std::uint32_t>&& data_assets);

    std::string name_;
    std::filesystem::path file_path_;
    const TypeRegistry& types_;
    AssetsFactory make_assets_;
    const TypeInfo* type_ = nullptr;
    std::unique_ptr<ValueBuffer> value_;
    std::unique_ptr<IAssetResolver> assets_;
    std::vector<std::uint32_t> data_assets_;
    // the value as save_data_asset writes it, and the value the file holds the same way (they differ while dirty)
    std::string value_text_;
    std::string file_value_text_;
    // the file as it was last read or written, so a change event for what was written is ignored
    std::string file_contents_;
    std::uint64_t reload_count_ = 0;
    std::atomic<bool> changed_ = false;
    // whether this asset registered its file with the watcher (so it unregisters it)
    bool watched_ = false;
};
}  // namespace engine
