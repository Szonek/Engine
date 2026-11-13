#include "terrain.h"
#include "file_io.h"
#include "logger.h"

#include <fmt/format.h>
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

engine::Terrain::Terrain(TerrainData data)
    : data_(std::make_shared<TerrainData>(std::move(data)))
{
    mark_saved();
}

void engine::Terrain::mark_saved()
{
    saved_revision_ = get_revision();
    saved_hash_ = data_->content_hash();
}

void engine::Terrain::refresh_dirty()
{
    if (is_dirty() && data_->content_hash() == saved_hash_)
    {
        saved_revision_ = get_revision();
    }
}

void engine::Terrain::replace_data(TerrainData data)
{
    revision_base_ += data_->get_revision();
    // a new object: whoever reads the old heights in place keeps them until it sees the new revision
    data_ = std::make_shared<TerrainData>(std::move(data));
}

void engine::Terrain::update_gpu()
{
    const auto changed_rows = data_->take_changed_rows();
    const auto width = data_->get_samples_x();
    const auto height = data_->get_samples_z();
    if (!height_texture_.is_valid() || texture_width_ != width || texture_height_ != height)
    {
        height_texture_ = Texture2D(width, height, false, data_->get_heights().data(), DataLayout::eR_FP32, TextureAddressClampMode::eClampToEdge);
        texture_width_ = width;
        texture_height_ = height;
        return;
    }
    if (changed_rows)
    {
        // whole rows: they are contiguous in the heights
        const auto [first, last] = *changed_rows;
        height_texture_.upload_region(0, first, width, last - first + 1, data_->get_heights().data() + static_cast<std::size_t>(first) * width, DataLayout::eR_FP32);
    }
}

std::optional<engine::TerrainData> engine::import_heightmap_file(const std::filesystem::path& path, float cell_size, float height_min, float height_max)
{
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == ".r16" || extension == ".raw")
    {
        const auto content = read_text_file(path, "Heightmap import");
        return content ? TerrainData::from_raw16(*content, cell_size, height_min, height_max) : std::nullopt;
    }
    if (extension != ".png")
    {
        log::log(log::LogLevel::eError, fmt::format("Heightmap import: {} is neither a .png nor a .r16 / .raw file.\n", path.string()));
        return std::nullopt;
    }
    const auto file = path.string();
    if (!stbi_is_16_bit(file.c_str()))
    {
        // an 8-bit heightmap has 256 heights: visible terraces
        log::log(log::LogLevel::eError, fmt::format("Heightmap import: {} can't be read or isn't a 16-bit image. Export the heightmap as 16-bit grayscale.\n", file));
        return std::nullopt;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    // one channel: the gray value (the first channel of a colour image)
    auto* pixels = stbi_load_16(file.c_str(), &width, &height, &channels, 1);
    if (!pixels || width <= 0 || height <= 0)
    {
        log::log(log::LogLevel::eError, fmt::format("Heightmap import: can't read {}.\n", file));
        stbi_image_free(pixels);
        return std::nullopt;
    }
    auto data = TerrainData::from_heightmap(std::span<const std::uint16_t>(pixels, static_cast<std::size_t>(width) * static_cast<std::size_t>(height)),
        static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), cell_size, height_min, height_max);
    stbi_image_free(pixels);
    return data;
}
