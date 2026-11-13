#pragma once
#include "terrain_data.h"

#include <glm/glm.hpp>

#include <optional>
#include <unordered_set>
#include <vector>

namespace engine
{
// The editor's terrain brushes (like Unity's Paint Terrain / Unreal's Landscape sculpt tools), free of the editor and
// of the graphics API: unit tested (terrain_brush_tests.cpp).

enum class TerrainBrushMode
{
    eRaise,
    eLower,
    // towards the average of the heights around each sample: takes out bumps and steps
    eSmooth,
    // towards one height (the height under the cursor when the stroke started): plateaus, paths, ground for buildings
    eFlatten,
};

struct TerrainBrush
{
    // metres, in the terrain's local xz plane
    float radius = 3.0f;
    // Raise and lower: metres per second at the brush's center. Smooth and flatten: how fast the heights approach their
    // target, per second (after 1 / strength seconds about two thirds of the way).
    float strength = 2.0f;
    // the part of the radius over which the effect fades out, from the edge inwards: 0 is a hard disc, 1 fades from
    // the center
    float falloff = 0.5f;
};

// how much of the brush's effect a sample at `distance` from its center gets: 1 in the middle, 0 from the radius on
float brush_weight(const TerrainBrush& brush, float distance);

// the samples within the brush's radius of `center` (the terrain's local x and z), nullopt when it is beside the terrain
std::optional<TerrainData::Region> brush_region(const TerrainData& terrain, const glm::vec2& center, float radius);

// One step of a stroke: the brush held at `center` for dt_seconds. Applying it twice for half the time gives the same
// heights as once for the whole time (to rounding), so a stroke doesn't depend on the frame rate. `flatten_height` is
// eFlatten's target. The samples it changed (the brush's region), nullopt when it touched none.
std::optional<TerrainData::Region> apply_brush(TerrainData& terrain, TerrainBrushMode mode, const TerrainBrush& brush, const glm::vec2& center,
    float dt_seconds, float flatten_height = 0.0f);

// A rectangle of a terrain's heights before and after a change (row by row), what an undo step of a stroke is made of.
struct TerrainPatch
{
    TerrainData::Region region;
    std::vector<float> before;
    std::vector<float> after;
};

// What a stroke changed, for its undo step: the heights before the stroke of every tile (TILE_SAMPLES samples a side)
// the stroke touched, so its cost and its step's size follow what the brush covered, not the rectangle around the
// whole stroke. Call include() with each region before changing it.
class TerrainStroke
{
public:
    static constexpr std::uint32_t TILE_SAMPLES = TERRAIN_CHUNK_CELLS;

    // `region` (inside the terrain) is about to change: the tiles it touches which the stroke didn't touch before are
    // recorded as they are now
    void include(const TerrainData& terrain, const TerrainData::Region& region);

    bool is_empty() const { return patches_.empty(); }
    // the tiles recorded so far; `after` is filled by finish()
    const std::vector<TerrainPatch>& get_patches() const { return patches_; }
    // The stroke ended: its tiles with the terrain's heights now as `after`, without the ones it left as they were.
    // The stroke is empty afterwards.
    std::vector<TerrainPatch> finish(const TerrainData& terrain);

private:
    std::vector<TerrainPatch> patches_;
    // the tiles recorded: z * 2^32 + x of each
    std::unordered_set<std::uint64_t> tiles_;
};
}  // namespace engine
