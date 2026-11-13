#pragma once
#include "graphics.h"
#include "terrain_data.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

namespace engine
{
// A terrain asset: the heights (TerrainData) and their copy on the GPU, a texture with one float per sample which the
// terrain's vertex shader displaces a shared grid with (terrain.vs), so a change of heights is an upload of the changed
// rows, never a mesh rebuild.
class Terrain
{
public:
    explicit Terrain(TerrainData data);
    // whoever reads its heights in place learns that it went
    ~Terrain() { TerrainData::mark_changed(); }
    Terrain(const Terrain&) = delete;
    Terrain& operator=(const Terrain&) = delete;

    const TerrainData& get_data() const { return *data_; }
    // The heights, for what reads them in place (a collision shape): shared, so they outlive a release of the asset.
    // After replace_data() it is another object; get_revision() tells.
    std::shared_ptr<const TerrainData> get_shared_data() const { return data_; }
    // the heights, to change them in place (the editor's brushes); every change moves the revision like set_heights
    TerrainData& get_mutable_data() { return *data_; }
    // many changes of the heights as one for the collision, see TerrainData::EditBatch
    std::unique_ptr<TerrainData::EditBatch> begin_edit_batch() { return std::make_unique<TerrainData::EditBatch>(data_); }
    // see TerrainData::set_heights
    bool set_heights(const TerrainData::Region& region, std::span<const float> heights) { return data_->set_heights(region, heights); }
    // new heights, possibly of another size (a heightmap imported again)
    void replace_data(TerrainData data);
    // moves with every change of the heights, also a replacement
    std::uint64_t get_revision() const { return revision_base_ + data_->get_revision(); }

    // Whether the heights differ from what the terrain was loaded with, made with or last saved as: what the editor's
    // unsaved changes are (like DataAsset::is_dirty). Cheap: a change makes it dirty, and whoever puts heights back
    // (an undo) asks for the comparison with refresh_dirty().
    bool is_dirty() const { return get_revision() != saved_revision_; }
    // the heights are what the terrain's file holds now
    void mark_saved();
    // Clean again when the heights are what was saved (a stroke undone back to the saved heights), by their content.
    // Reads every height, so it is for after a change, not for every frame.
    void refresh_dirty();

    // Brings the height texture up to date; before the frame is rendered (Application::end_frame).
    void update_gpu();
    // valid after the first update_gpu(); the object stays the same for the terrain's life
    const Texture2D& get_height_texture() const { return height_texture_; }

private:
    std::shared_ptr<TerrainData> data_;
    // the revisions of the heights replaced so far, so the revision never repeats
    std::uint64_t revision_base_ = 0;
    std::uint64_t saved_revision_ = 0;
    // TerrainData::content_hash of what was saved
    std::uint64_t saved_hash_ = 0;
    Texture2D height_texture_;
    std::uint32_t texture_width_ = 0;
    std::uint32_t texture_height_ = 0;
};

// The heights of a heightmap file: a 16-bit grayscale PNG, or a raw 16-bit file (.r16, .raw), see
// TerrainData::from_heightmap. nullopt (logged) when the file can't be read or is neither.
std::optional<TerrainData> import_heightmap_file(const std::filesystem::path& path, float cell_size, float height_min, float height_max);
}  // namespace engine
