#include "terrain_data.h"
#include "logger.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

// the file and .r16 heightmaps are little endian, read and written as the host's bytes
static_assert(std::endian::native == std::endian::little, "terrain files are little endian");

namespace
{
constexpr std::array<char, 8> FILE_MAGIC = { 'E', 'N', 'G', 'T', 'E', 'R', 'R', '\0' };
constexpr std::uint32_t FILE_VERSION = 1;

struct FileHeader
{
    std::array<char, 8> magic;
    std::uint32_t version;
    std::uint32_t samples_x;
    std::uint32_t samples_z;
    float cell_size;
};
static_assert(sizeof(FileHeader) == 24);

bool is_valid_size(std::uint32_t samples_x, std::uint32_t samples_z, float cell_size, std::string_view what)
{
    if (samples_x < 2 || samples_z < 2 || samples_x > engine::TERRAIN_MAX_SAMPLES || samples_z > engine::TERRAIN_MAX_SAMPLES)
    {
        engine::log::log(engine::log::LogLevel::eError, fmt::format("{}: {} x {} samples, a terrain has 2 to {} samples a side.\n",
            what, samples_x, samples_z, engine::TERRAIN_MAX_SAMPLES));
        return false;
    }
    if (!std::isfinite(cell_size) || cell_size <= 0.0f)
    {
        engine::log::log(engine::log::LogLevel::eError, fmt::format("{}: the cell size has to be positive (it is {}).\n", what, cell_size));
        return false;
    }
    return true;
}

bool all_finite(std::span<const float> values)
{
    return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
}
}  // namespace

std::optional<engine::TerrainData> engine::TerrainData::create(std::uint32_t samples_x, std::uint32_t samples_z, float cell_size, std::span<const float> heights)
{
    if (!is_valid_size(samples_x, samples_z, cell_size, "Terrain"))
    {
        return std::nullopt;
    }
    const auto count = static_cast<std::size_t>(samples_x) * samples_z;
    if (!heights.empty() && heights.size() != count)
    {
        log::log(log::LogLevel::eError, fmt::format("Terrain: {} heights given for {} x {} samples.\n", heights.size(), samples_x, samples_z));
        return std::nullopt;
    }
    if (!all_finite(heights))
    {
        log::log(log::LogLevel::eError, "Terrain: the heights have to be finite numbers.\n");
        return std::nullopt;
    }
    TerrainData data;
    data.samples_x_ = samples_x;
    data.samples_z_ = samples_z;
    data.cell_size_ = cell_size;
    data.heights_.assign(count, 0.0f);
    if (!heights.empty())
    {
        std::copy(heights.begin(), heights.end(), data.heights_.begin());
    }
    data.chunks_x_ = (samples_x - 1 + TERRAIN_CHUNK_CELLS - 1) / TERRAIN_CHUNK_CELLS;
    data.chunks_z_ = (samples_z - 1 + TERRAIN_CHUNK_CELLS - 1) / TERRAIN_CHUNK_CELLS;
    data.chunks_.resize(static_cast<std::size_t>(data.chunks_x_) * data.chunks_z_);
    for (std::uint32_t cz = 0; cz < data.chunks_z_; cz++)
    {
        for (std::uint32_t cx = 0; cx < data.chunks_x_; cx++)
        {
            auto& chunk = data.chunks_[static_cast<std::size_t>(cz) * data.chunks_x_ + cx];
            chunk.x = cx * TERRAIN_CHUNK_CELLS;
            chunk.z = cz * TERRAIN_CHUNK_CELLS;
        }
    }
    data.update_bounds(Region{ 0, 0, samples_x, samples_z });
    data.mark_rows_changed(0, samples_z - 1);
    mark_changed();
    return data;
}

std::optional<engine::TerrainData> engine::TerrainData::from_heightmap(std::span<const std::uint16_t> pixels, std::uint32_t width, std::uint32_t height,
    float cell_size, float height_min, float height_max)
{
    if (!is_valid_size(width, height, cell_size, "Heightmap"))
    {
        return std::nullopt;
    }
    if (pixels.size() != static_cast<std::size_t>(width) * height)
    {
        log::log(log::LogLevel::eError, fmt::format("Heightmap: {} pixels given for {} x {}.\n", pixels.size(), width, height));
        return std::nullopt;
    }
    if (!std::isfinite(height_min) || !std::isfinite(height_max) || height_max < height_min)
    {
        log::log(log::LogLevel::eError, fmt::format("Heightmap: the height range {} .. {} isn't one.\n", height_min, height_max));
        return std::nullopt;
    }
    std::vector<float> heights(pixels.size());
    const float scale = (height_max - height_min) / 65535.0f;
    std::transform(pixels.begin(), pixels.end(), heights.begin(), [height_min, scale](std::uint16_t pixel) { return height_min + static_cast<float>(pixel) * scale; });
    return create(width, height, cell_size, heights);
}

std::optional<engine::TerrainData> engine::TerrainData::from_raw16(std::string_view content, float cell_size, float height_min, float height_max)
{
    const auto pixel_count = content.size() / sizeof(std::uint16_t);
    const auto side = static_cast<std::size_t>(std::llround(std::sqrt(static_cast<double>(pixel_count))));
    if (content.empty() || content.size() % sizeof(std::uint16_t) != 0 || side * side != pixel_count)
    {
        log::log(log::LogLevel::eError, fmt::format("Heightmap: {} bytes aren't a square 16-bit raw heightmap.\n", content.size()));
        return std::nullopt;
    }
    std::vector<std::uint16_t> pixels(pixel_count);
    std::memcpy(pixels.data(), content.data(), content.size());
    return from_heightmap(pixels, static_cast<std::uint32_t>(side), static_cast<std::uint32_t>(side), cell_size, height_min, height_max);
}

std::string engine::TerrainData::serialize() const
{
    const FileHeader header{ FILE_MAGIC, FILE_VERSION, samples_x_, samples_z_, cell_size_ };
    std::string content(sizeof(header) + heights_.size() * sizeof(float), '\0');
    std::memcpy(content.data(), &header, sizeof(header));
    std::memcpy(content.data() + sizeof(header), heights_.data(), heights_.size() * sizeof(float));
    return content;
}

std::optional<engine::TerrainData> engine::TerrainData::parse(std::string_view content, std::string_view what)
{
    FileHeader header{};
    if (content.size() < sizeof(header))
    {
        log::log(log::LogLevel::eError, fmt::format("{}: not a terrain file (too short).\n", what));
        return std::nullopt;
    }
    std::memcpy(&header, content.data(), sizeof(header));
    if (header.magic != FILE_MAGIC)
    {
        log::log(log::LogLevel::eError, fmt::format("{}: not a terrain file.\n", what));
        return std::nullopt;
    }
    if (header.version != FILE_VERSION)
    {
        log::log(log::LogLevel::eError, fmt::format("{}: terrain file version {}, this build reads version {}.\n", what, header.version, FILE_VERSION));
        return std::nullopt;
    }
    if (!is_valid_size(header.samples_x, header.samples_z, header.cell_size, what))
    {
        return std::nullopt;
    }
    const auto count = static_cast<std::size_t>(header.samples_x) * header.samples_z;
    if (content.size() != sizeof(header) + count * sizeof(float))
    {
        log::log(log::LogLevel::eError, fmt::format("{}: the terrain file holds {} bytes of heights, {} x {} samples need {}.\n",
            what, content.size() - sizeof(header), header.samples_x, header.samples_z, count * sizeof(float)));
        return std::nullopt;
    }
    std::vector<float> heights(count);
    std::memcpy(heights.data(), content.data() + sizeof(header), count * sizeof(float));
    if (!all_finite(heights))
    {
        log::log(log::LogLevel::eError, fmt::format("{}: the terrain file holds heights which aren't finite numbers.\n", what));
        return std::nullopt;
    }
    return create(header.samples_x, header.samples_z, header.cell_size, heights);
}

bool engine::TerrainData::get_heights(const Region& region, std::span<float> out) const
{
    if (region.width == 0 || region.depth == 0 || region.x >= samples_x_ || region.z >= samples_z_
        || region.width > samples_x_ - region.x || region.depth > samples_z_ - region.z
        || out.size() != static_cast<std::size_t>(region.width) * region.depth)
    {
        return false;
    }
    for (std::uint32_t row = 0; row < region.depth; row++)
    {
        const auto* source = heights_.data() + static_cast<std::size_t>(region.z + row) * samples_x_ + region.x;
        std::copy(source, source + region.width, out.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(row) * region.width));
    }
    return true;
}

bool engine::TerrainData::set_heights(const Region& region, std::span<const float> heights)
{
    if (region.width == 0 || region.depth == 0 || region.x >= samples_x_ || region.z >= samples_z_
        || region.width > samples_x_ - region.x || region.depth > samples_z_ - region.z)
    {
        log::log(log::LogLevel::eError, fmt::format("Terrain: the region at ({}, {}) of {} x {} samples isn't inside the terrain's {} x {}.\n",
            region.x, region.z, region.width, region.depth, samples_x_, samples_z_));
        return false;
    }
    if (heights.size() != static_cast<std::size_t>(region.width) * region.depth)
    {
        log::log(log::LogLevel::eError, fmt::format("Terrain: {} heights given for a region of {} x {} samples.\n", heights.size(), region.width, region.depth));
        return false;
    }
    if (!all_finite(heights))
    {
        log::log(log::LogLevel::eError, "Terrain: the heights have to be finite numbers.\n");
        return false;
    }
    for (std::uint32_t row = 0; row < region.depth; row++)
    {
        const auto* source = heights.data() + static_cast<std::size_t>(row) * region.width;
        std::copy(source, source + region.width, heights_.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(region.z + row) * samples_x_ + region.x));
    }
    update_bounds(region);
    mark_rows_changed(region.z, region.z + region.depth - 1);
    revision_++;
    mark_changed();
    return true;
}

std::optional<engine::TerrainData::CellSample> engine::TerrainData::sample_cell(float local_x, float local_z) const
{
    const float grid_x = local_x / cell_size_;
    const float grid_z = local_z / cell_size_;
    const auto max_x = static_cast<float>(samples_x_ - 1);
    const auto max_z = static_cast<float>(samples_z_ - 1);
    // also false for NaN
    if (!(grid_x >= 0.0f && grid_z >= 0.0f && grid_x <= max_x && grid_z <= max_z))
    {
        return std::nullopt;
    }
    // a point on the last edge belongs to the last cell
    const auto cell_x = std::min(static_cast<std::uint32_t>(grid_x), samples_x_ - 2);
    const auto cell_z = std::min(static_cast<std::uint32_t>(grid_z), samples_z_ - 2);
    return CellSample{
        .h00 = get_height(cell_x, cell_z),
        .h10 = get_height(cell_x + 1, cell_z),
        .h01 = get_height(cell_x, cell_z + 1),
        .h11 = get_height(cell_x + 1, cell_z + 1),
        .fx = grid_x - static_cast<float>(cell_x),
        .fz = grid_z - static_cast<float>(cell_z) };
}

std::optional<float> engine::TerrainData::sample_height(float local_x, float local_z) const
{
    const auto cell = sample_cell(local_x, local_z);
    if (!cell)
    {
        return std::nullopt;
    }
    // the cell's two triangles meet on the diagonal fx + fz = 1
    if (cell->fx + cell->fz <= 1.0f)
    {
        return cell->h00 + cell->fx * (cell->h10 - cell->h00) + cell->fz * (cell->h01 - cell->h00);
    }
    return cell->h11 + (1.0f - cell->fx) * (cell->h01 - cell->h11) + (1.0f - cell->fz) * (cell->h10 - cell->h11);
}

std::optional<glm::vec3> engine::TerrainData::sample_normal(float local_x, float local_z) const
{
    const auto cell = sample_cell(local_x, local_z);
    if (!cell)
    {
        return std::nullopt;
    }
    // the height's change along x and z over one cell, on the triangle under the point
    const bool first_triangle = cell->fx + cell->fz <= 1.0f;
    const float slope_x = first_triangle ? cell->h10 - cell->h00 : cell->h11 - cell->h01;
    const float slope_z = first_triangle ? cell->h01 - cell->h00 : cell->h11 - cell->h10;
    return glm::normalize(glm::vec3(-slope_x, cell_size_, -slope_z));
}

engine::Aabb engine::TerrainData::get_bounds() const
{
    const auto size = get_size();
    return Aabb{ glm::vec3(0.0f, min_height_, 0.0f), glm::vec3(size.x, max_height_, size.y) };
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> engine::TerrainData::take_changed_rows()
{
    if (!rows_changed_)
    {
        return std::nullopt;
    }
    rows_changed_ = false;
    return std::pair{ first_changed_row_, last_changed_row_ };
}

void engine::TerrainData::mark_rows_changed(std::uint32_t first, std::uint32_t last)
{
    first_changed_row_ = rows_changed_ ? std::min(first_changed_row_, first) : first;
    last_changed_row_ = rows_changed_ ? std::max(last_changed_row_, last) : last;
    rows_changed_ = true;
}

void engine::TerrainData::update_bounds(const Region& region)
{
    // a sample on a chunk's edge is the previous chunk's last one too
    const auto first_chunk_x = region.x > 0 ? (region.x - 1) / TERRAIN_CHUNK_CELLS : 0;
    const auto first_chunk_z = region.z > 0 ? (region.z - 1) / TERRAIN_CHUNK_CELLS : 0;
    const auto last_chunk_x = std::min((region.x + region.width - 1) / TERRAIN_CHUNK_CELLS, chunks_x_ - 1);
    const auto last_chunk_z = std::min((region.z + region.depth - 1) / TERRAIN_CHUNK_CELLS, chunks_z_ - 1);
    for (auto cz = first_chunk_z; cz <= last_chunk_z; cz++)
    {
        for (auto cx = first_chunk_x; cx <= last_chunk_x; cx++)
        {
            auto& chunk = chunks_[static_cast<std::size_t>(cz) * chunks_x_ + cx];
            const auto end_x = std::min(chunk.x + TERRAIN_CHUNK_CELLS, samples_x_ - 1);
            const auto end_z = std::min(chunk.z + TERRAIN_CHUNK_CELLS, samples_z_ - 1);
            auto low = std::numeric_limits<float>::max();
            auto high = std::numeric_limits<float>::lowest();
            for (auto z = chunk.z; z <= end_z; z++)
            {
                const auto* row = heights_.data() + static_cast<std::size_t>(z) * samples_x_;
                const auto [row_low, row_high] = std::minmax_element(row + chunk.x, row + end_x + 1);
                low = std::min(low, *row_low);
                high = std::max(high, *row_high);
            }
            chunk.bounds.min = glm::vec3(static_cast<float>(chunk.x) * cell_size_, low, static_cast<float>(chunk.z) * cell_size_);
            chunk.bounds.max = glm::vec3(static_cast<float>(end_x) * cell_size_, high, static_cast<float>(end_z) * cell_size_);
        }
    }
    min_height_ = std::numeric_limits<float>::max();
    max_height_ = std::numeric_limits<float>::lowest();
    for (const auto& chunk : chunks_)
    {
        min_height_ = std::min(min_height_, chunk.bounds.min.y);
        max_height_ = std::max(max_height_, chunk.bounds.max.y);
    }
}

void engine::TerrainData::append_triangles(const glm::mat4& matrix, std::vector<float>& vertices, std::vector<std::int32_t>& triangles) const
{
    const auto base = static_cast<std::int32_t>(vertices.size() / 3);
    vertices.reserve(vertices.size() + heights_.size() * 3);
    for (std::uint32_t z = 0; z < samples_z_; z++)
    {
        for (std::uint32_t x = 0; x < samples_x_; x++)
        {
            const auto position = glm::vec3(matrix * glm::vec4(static_cast<float>(x) * cell_size_, get_height(x, z), static_cast<float>(z) * cell_size_, 1.0f));
            vertices.insert(vertices.end(), { position.x, position.y, position.z });
        }
    }
    triangles.reserve(triangles.size() + static_cast<std::size_t>(samples_x_ - 1) * (samples_z_ - 1) * 6);
    for (std::uint32_t z = 0; z + 1 < samples_z_; z++)
    {
        for (std::uint32_t x = 0; x + 1 < samples_x_; x++)
        {
            const auto i00 = base + static_cast<std::int32_t>(z * samples_x_ + x);
            const auto i10 = i00 + 1;
            const auto i01 = i00 + static_cast<std::int32_t>(samples_x_);
            const auto i11 = i01 + 1;
            triangles.insert(triangles.end(), { i00, i01, i10, i10, i01, i11 });
        }
    }
}
