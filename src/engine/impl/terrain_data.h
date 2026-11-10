#pragma once
#include "bounds.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine
{
// a chunk is this many cells along x and z: the unit terrains are culled by (and, later, carry their vegetation by)
inline constexpr std::uint32_t TERRAIN_CHUNK_CELLS = 32;
// samples along one side: at most 8192 cells
inline constexpr std::uint32_t TERRAIN_MAX_SAMPLES = 8193;

// The heights of a terrain (like Unity's TerrainData): a grid of samples_x * samples_z heights in metres, cell_size
// metres apart. Free of the graphics API and of physics, unit tested (terrain_data_tests.cpp).
//
// Local space: sample (x, z) is at (x * cell_size, height, z * cell_size), so the terrain starts at its origin and
// extends along +x and +z; the transform of its game object places it.
// Every cell is two triangles split along the diagonal from sample (x, z + 1) to (x + 1, z). Rendering, collision
// (Bullet's heightfield) and the nav mesh input all use this split, and sample_height() is the height of that surface.
class TerrainData
{
public:
    // a rectangle of samples
    struct Region
    {
        std::uint32_t x = 0;
        std::uint32_t z = 0;
        std::uint32_t width = 0;
        std::uint32_t depth = 0;
    };

    struct Chunk
    {
        // its first sample; its last one is the first of the next chunk (chunks share their edge samples)
        std::uint32_t x = 0;
        std::uint32_t z = 0;
        // in the terrain's local space, around the chunk's surface
        Aabb bounds;
    };

    // A terrain of the given size: flat at height 0, or with `heights` (row by row: samples_x heights for z = 0, then
    // z = 1, ...). nullopt (logged) for a size out of range (2..TERRAIN_MAX_SAMPLES samples a side), a cell size which
    // isn't positive, or heights of the wrong count or not finite.
    static std::optional<TerrainData> create(std::uint32_t samples_x, std::uint32_t samples_z, float cell_size, std::span<const float> heights = {});
    // From a 16-bit grayscale heightmap (what Gaea, World Machine and Blender export): pixel (x, y) is sample (x, z = y),
    // 0 is height_min and 65535 height_max.
    static std::optional<TerrainData> from_heightmap(std::span<const std::uint16_t> pixels, std::uint32_t width, std::uint32_t height, float cell_size,
        float height_min, float height_max);
    // From the content of a .r16 / .raw file: a square 16-bit little endian heightmap without a header, its side found
    // from the size.
    static std::optional<TerrainData> from_raw16(std::string_view content, float cell_size, float height_min, float height_max);

    // The content of a terrain file and back. The file is binary, little endian: "ENGTERR\0", version (uint32),
    // samples_x, samples_z (uint32), cell_size (float), then the heights (float), row by row. parse() gives nullopt
    // (logged, `what` names the file) for anything else.
    std::string serialize() const;
    static std::optional<TerrainData> parse(std::string_view content, std::string_view what);

    std::uint32_t get_samples_x() const { return samples_x_; }
    std::uint32_t get_samples_z() const { return samples_z_; }
    float get_cell_size() const { return cell_size_; }
    // metres along x and z
    glm::vec2 get_size() const { return glm::vec2(static_cast<float>(samples_x_ - 1), static_cast<float>(samples_z_ - 1)) * cell_size_; }
    // of all heights
    float get_min_height() const { return min_height_; }
    float get_max_height() const { return max_height_; }

    // every height, row by row. The buffer is never reallocated (collision reads it in place).
    std::span<const float> get_heights() const { return heights_; }
    float get_height(std::uint32_t x, std::uint32_t z) const { return heights_[static_cast<std::size_t>(z) * samples_x_ + x]; }
    // the heights of a region, row by row, into `out` (region.width * region.depth); false (nothing written) when the
    // region isn't inside the terrain or `out` has another size
    bool get_heights(const Region& region, std::span<float> out) const;
    // Replaces the heights of a region. False (logged, nothing changed) when the region isn't inside the terrain, or the
    // heights have another count or aren't finite. Updates the bounds of the chunks it touches and moves the revision.
    bool set_heights(const Region& region, std::span<const float> heights);

    // The height of the surface at a point of the local xz plane, nullopt outside the terrain.
    std::optional<float> sample_height(float local_x, float local_z) const;
    // the surface's normal there (of the triangle under the point), in local space
    std::optional<glm::vec3> sample_normal(float local_x, float local_z) const;

    // row by row, get_chunks_x() a row
    std::span<const Chunk> get_chunks() const { return chunks_; }
    std::uint32_t get_chunks_x() const { return chunks_x_; }
    std::uint32_t get_chunks_z() const { return chunks_z_; }
    // around the whole surface, in local space
    Aabb get_bounds() const;

    // moves with every change of the heights: what is derived from them (the GPU's copy, the collision shape's range)
    // is brought up to date when it differs
    std::uint64_t get_revision() const { return revision_; }
    // The rows of samples changed since the last call, as [first, last], for uploading them; nullopt when none did.
    std::optional<std::pair<std::uint32_t, std::uint32_t>> take_changed_rows();

    // The triangles of the surface, the input of a nav mesh build: appends positions in the space `matrix` maps the
    // local space to (xyz triples) and indices into them (counter-clockwise seen from above, so normals point up).
    void append_triangles(const glm::mat4& matrix, std::vector<float>& vertices, std::vector<std::int32_t>& triangles) const;

private:
    TerrainData() = default;
    // the bounds of the chunks overlapping the region of samples, and the min and max of all heights
    void update_bounds(const Region& region);
    void mark_rows_changed(std::uint32_t first, std::uint32_t last);
    // the heights of the corners of the cell containing the point and where the point is in it (0..1)
    struct CellSample
    {
        float h00, h10, h01, h11;
        float fx, fz;
    };
    std::optional<CellSample> sample_cell(float local_x, float local_z) const;

    std::uint32_t samples_x_ = 0;
    std::uint32_t samples_z_ = 0;
    float cell_size_ = 1.0f;
    std::vector<float> heights_;
    float min_height_ = 0.0f;
    float max_height_ = 0.0f;
    std::uint32_t chunks_x_ = 0;
    std::uint32_t chunks_z_ = 0;
    std::vector<Chunk> chunks_;
    std::uint64_t revision_ = 1;
    bool rows_changed_ = false;
    std::uint32_t first_changed_row_ = 0;
    std::uint32_t last_changed_row_ = 0;
};
}  // namespace engine
