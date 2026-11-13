#include "terrain_brush.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

float engine::brush_weight(const TerrainBrush& brush, float distance)
{
    if (!(brush.radius > 0.0f) || distance >= brush.radius)
    {
        return 0.0f;
    }
    const float falloff = std::clamp(brush.falloff, 0.0f, 1.0f);
    const float inner = brush.radius * (1.0f - falloff);
    if (distance <= inner)
    {
        return 1.0f;
    }
    // smoothstep from the inner radius down to the edge
    const float t = (brush.radius - distance) / (brush.radius - inner);
    return t * t * (3.0f - 2.0f * t);
}

std::optional<engine::TerrainData::Region> engine::brush_region(const TerrainData& terrain, const glm::vec2& center, float radius)
{
    if (!(radius > 0.0f))
    {
        return std::nullopt;
    }
    const float cell = terrain.get_cell_size();
    const auto first = [cell](float low) { return static_cast<std::int64_t>(std::ceil(low / cell)); };
    const auto last = [cell](float high) { return static_cast<std::int64_t>(std::floor(high / cell)); };
    const auto x0 = std::max<std::int64_t>(first(center.x - radius), 0);
    const auto z0 = std::max<std::int64_t>(first(center.y - radius), 0);
    const auto x1 = std::min<std::int64_t>(last(center.x + radius), static_cast<std::int64_t>(terrain.get_samples_x()) - 1);
    const auto z1 = std::min<std::int64_t>(last(center.y + radius), static_cast<std::int64_t>(terrain.get_samples_z()) - 1);
    if (x0 > x1 || z0 > z1)
    {
        return std::nullopt;
    }
    return TerrainData::Region{ static_cast<std::uint32_t>(x0), static_cast<std::uint32_t>(z0), static_cast<std::uint32_t>(x1 - x0 + 1),
        static_cast<std::uint32_t>(z1 - z0 + 1) };
}

std::optional<engine::TerrainData::Region> engine::apply_brush(TerrainData& terrain, TerrainBrushMode mode, const TerrainBrush& brush, const glm::vec2& center,
    float dt_seconds, float flatten_height)
{
    const auto region = brush_region(terrain, center, brush.radius);
    if (!region || !(dt_seconds > 0.0f))
    {
        return std::nullopt;
    }
    std::vector<float> heights(static_cast<std::size_t>(region->width) * region->depth);
    terrain.get_heights(*region, heights);
    const float cell = terrain.get_cell_size();
    const auto last_x = terrain.get_samples_x() - 1;
    const auto last_z = terrain.get_samples_z() - 1;
    for (std::uint32_t row = 0; row < region->depth; row++)
    {
        for (std::uint32_t column = 0; column < region->width; column++)
        {
            const auto x = region->x + column;
            const auto z = region->z + row;
            const float distance = glm::distance(glm::vec2(static_cast<float>(x), static_cast<float>(z)) * cell, center);
            const float weight = brush_weight(brush, distance);
            if (weight <= 0.0f)
            {
                continue;
            }
            auto& height = heights[static_cast<std::size_t>(row) * region->width + column];
            // how far towards its target a height gets in dt: exponential, so two half steps are one whole step
            const float approach = 1.0f - std::exp(-std::max(brush.strength, 0.0f) * weight * dt_seconds);
            switch (mode)
            {
            case TerrainBrushMode::eRaise:
                height += brush.strength * weight * dt_seconds;
                break;
            case TerrainBrushMode::eLower:
                height -= brush.strength * weight * dt_seconds;
                break;
            case TerrainBrushMode::eFlatten:
                height += (flatten_height - height) * approach;
                break;
            case TerrainBrushMode::eSmooth:
            {
                // the average of the sample and its neighbours (fewer on the terrain's edges), from the heights before
                // this step: every sample is smoothed against the same surface
                float sum = 0.0f;
                std::uint32_t count = 0;
                for (auto nz = z > 0 ? z - 1 : z; nz <= std::min(z + 1, last_z); nz++)
                {
                    for (auto nx = x > 0 ? x - 1 : x; nx <= std::min(x + 1, last_x); nx++)
                    {
                        sum += terrain.get_height(nx, nz);
                        count++;
                    }
                }
                height += (sum / static_cast<float>(count) - height) * approach;
                break;
            }
            }
        }
    }
    return terrain.set_heights(*region, heights) ? region : std::nullopt;
}

void engine::TerrainStroke::include(const TerrainData& terrain, const TerrainData::Region& region)
{
    if (region.width == 0 || region.depth == 0 || region.x + region.width > terrain.get_samples_x() || region.z + region.depth > terrain.get_samples_z())
    {
        return;
    }
    for (std::uint32_t tile_z = region.z / TILE_SAMPLES; tile_z <= (region.z + region.depth - 1) / TILE_SAMPLES; tile_z++)
    {
        for (std::uint32_t tile_x = region.x / TILE_SAMPLES; tile_x <= (region.x + region.width - 1) / TILE_SAMPLES; tile_x++)
        {
            if (!tiles_.insert((static_cast<std::uint64_t>(tile_z) << 32) | tile_x).second)
            {
                // recorded when the stroke first touched it: those are its heights before the stroke
                continue;
            }
            TerrainPatch patch;
            patch.region.x = tile_x * TILE_SAMPLES;
            patch.region.z = tile_z * TILE_SAMPLES;
            patch.region.width = std::min(TILE_SAMPLES, terrain.get_samples_x() - patch.region.x);
            patch.region.depth = std::min(TILE_SAMPLES, terrain.get_samples_z() - patch.region.z);
            patch.before.resize(static_cast<std::size_t>(patch.region.width) * patch.region.depth);
            terrain.get_heights(patch.region, patch.before);
            patches_.push_back(std::move(patch));
        }
    }
}

std::vector<engine::TerrainPatch> engine::TerrainStroke::finish(const TerrainData& terrain)
{
    auto patches = std::move(patches_);
    patches_.clear();
    tiles_.clear();
    for (auto& patch : patches)
    {
        patch.after.resize(patch.before.size());
        if (!terrain.get_heights(patch.region, patch.after))
        {
            // the terrain got another size under the stroke: nothing of it can be given back
            return {};
        }
    }
    std::erase_if(patches, [](const TerrainPatch& patch) { return patch.before == patch.after; });
    return patches;
}
