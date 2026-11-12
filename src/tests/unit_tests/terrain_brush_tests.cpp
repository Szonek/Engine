#include "editor_terrain_stroke.h"
#include "terrain_brush.h"
#include "test_logger.h"

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

// The editor's terrain tools without the editor: the ray finding the cursor on a terrain, the brushes, what a stroke
// records for its undo step, and that step.
namespace
{
using engine::TerrainBrush;
using engine::TerrainBrushMode;
using engine::TerrainData;

TerrainData flat(std::uint32_t samples = 33, float cell_size = 1.0f, float height = 0.0f)
{
    return *TerrainData::create(samples, samples, cell_size, std::vector<float>(static_cast<std::size_t>(samples) * samples, height));
}

// rises 1 m per metre along x
TerrainData slope(std::uint32_t samples = 17, float cell_size = 1.0f)
{
    std::vector<float> heights;
    for (std::uint32_t z = 0; z < samples; z++)
    {
        for (std::uint32_t x = 0; x < samples; x++)
        {
            heights.push_back(static_cast<float>(x) * cell_size);
        }
    }
    return *TerrainData::create(samples, samples, cell_size, heights);
}

std::vector<float> all_heights(const TerrainData& terrain)
{
    return std::vector<float>(terrain.get_heights().begin(), terrain.get_heights().end());
}
}  // namespace

// -- the ray ---------------------------------------------------------------------------------------------------------

TEST(TerrainRaycast, RayFromAboveHitsTheSurfaceUnderIt)
{
    const auto terrain = slope(17, 2.0f);
    const auto hit = terrain.raycast({ 7.3f, 100.0f, 11.1f }, { 0.0f, -1.0f, 0.0f }, 500.0f);
    ASSERT_TRUE(hit);
    EXPECT_NEAR(hit->x, 7.3f, 1e-4f);
    EXPECT_NEAR(hit->z, 11.1f, 1e-4f);
    EXPECT_NEAR(hit->y, 7.3f, 1e-3f);
    EXPECT_NEAR(hit->y, *terrain.sample_height(hit->x, hit->z), 1e-3f);
}

TEST(TerrainRaycast, SlantedRayHitsWhereItMeetsTheSurface)
{
    const auto terrain = flat(33, 1.0f, 2.0f);
    // down at 45 degrees along +x and +z from (1, 12, 3): meets y = 2 after 10 m down
    const glm::vec3 direction(std::sqrt(0.5f), -1.0f, std::sqrt(0.5f));
    const auto hit = terrain.raycast({ 1.0f, 12.0f, 3.0f }, direction, 100.0f);
    ASSERT_TRUE(hit);
    EXPECT_NEAR(hit->y, 2.0f, 1e-3f);
    EXPECT_NEAR(hit->x, 1.0f + 10.0f * std::sqrt(0.5f), 1e-2f);
    EXPECT_NEAR(hit->z, 3.0f + 10.0f * std::sqrt(0.5f), 1e-2f);
}

TEST(TerrainRaycast, FirstSurfaceAlongTheRayIsHit)
{
    // a wall across the terrain at x = 8..9, 5 m high: a low ray along +x stops at its near side
    std::vector<float> heights(17 * 17, 0.0f);
    for (std::uint32_t z = 0; z < 17; z++)
    {
        heights[z * 17 + 8] = 5.0f;
        heights[z * 17 + 9] = 5.0f;
    }
    const auto terrain = *TerrainData::create(17, 17, 1.0f, heights);
    const auto hit = terrain.raycast({ 1.0f, 2.5f, 6.5f }, { 1.0f, 0.0f, 0.0f }, 100.0f);
    ASSERT_TRUE(hit);
    // the wall rises from x = 7 to 8: at half its height
    EXPECT_NEAR(hit->x, 7.5f, 1e-2f);
    EXPECT_NEAR(hit->y, 2.5f, 1e-3f);
    // the same ray from the other side stops at the far side
    const auto back = terrain.raycast({ 15.0f, 2.5f, 6.5f }, { -1.0f, 0.0f, 0.0f }, 100.0f);
    ASSERT_TRUE(back);
    EXPECT_NEAR(back->x, 9.5f, 1e-2f);
}

TEST(TerrainRaycast, MissesBesideAboveAndBeyondItsLength)
{
    const auto terrain = flat(17, 1.0f, 1.0f);
    // beside the terrain
    EXPECT_FALSE(terrain.raycast({ -3.0f, 10.0f, 5.0f }, { 0.0f, -1.0f, 0.0f }, 100.0f));
    EXPECT_FALSE(terrain.raycast({ 5.0f, 10.0f, 16.5f }, { 0.0f, -1.0f, 0.0f }, 100.0f));
    // pointing away
    EXPECT_FALSE(terrain.raycast({ 5.0f, 10.0f, 5.0f }, { 0.0f, 1.0f, 0.0f }, 100.0f));
    // too short to reach
    EXPECT_FALSE(terrain.raycast({ 5.0f, 10.0f, 5.0f }, { 0.0f, -1.0f, 0.0f }, 8.9f));
    EXPECT_TRUE(terrain.raycast({ 5.0f, 10.0f, 5.0f }, { 0.0f, -1.0f, 0.0f }, 9.1f));
    // parallel to it, above
    EXPECT_FALSE(terrain.raycast({ -5.0f, 3.0f, 5.0f }, { 1.0f, 0.0f, 0.0f }, 100.0f));
    // no direction, no length
    EXPECT_FALSE(terrain.raycast({ 5.0f, 10.0f, 5.0f }, { 0.0f, 0.0f, 0.0f }, 100.0f));
    EXPECT_FALSE(terrain.raycast({ 5.0f, 10.0f, 5.0f }, { 0.0f, -1.0f, 0.0f }, 0.0f));
}

TEST(TerrainRaycast, RayStartingOutsideEntersThroughTheSide)
{
    const auto terrain = flat(17, 1.0f, 0.0f);
    // from beside and above the terrain, down across it
    const auto hit = terrain.raycast({ -10.0f, 5.0f, 8.0f }, { 2.0f, -0.5f, 0.0f }, 100.0f);
    ASSERT_TRUE(hit);
    EXPECT_NEAR(hit->x, 10.0f, 1e-2f);
    EXPECT_NEAR(hit->y, 0.0f, 1e-3f);
    EXPECT_NEAR(hit->z, 8.0f, 1e-3f);
}

TEST(TerrainRaycast, EveryPointUnderAGridOfRaysIsOnTheSurface)
{
    // bumps, so the two triangles of a cell differ; rays through cell edges, corners and diagonals included
    std::vector<float> heights;
    for (std::uint32_t z = 0; z < 9; z++)
    {
        for (std::uint32_t x = 0; x < 9; x++)
        {
            heights.push_back(std::sin(static_cast<float>(x) * 1.3f) + std::cos(static_cast<float>(z) * 0.9f) * 2.0f);
        }
    }
    const auto terrain = *TerrainData::create(9, 9, 1.5f, heights);
    for (float x = 0.0f; x <= 12.0f; x += 0.25f)
    {
        for (float z = 0.0f; z <= 12.0f; z += 0.25f)
        {
            const auto hit = terrain.raycast({ x, 50.0f, z }, { 0.0f, -1.0f, 0.0f }, 100.0f);
            ASSERT_TRUE(hit) << x << ", " << z;
            EXPECT_NEAR(hit->y, *terrain.sample_height(x, z), 1e-3f) << x << ", " << z;
        }
    }
}

// -- the brushes -----------------------------------------------------------------------------------------------------

TEST(TerrainBrushWeight, FullInTheMiddleFadingToTheEdge)
{
    const TerrainBrush brush{ 4.0f, 1.0f, 0.5f };
    EXPECT_FLOAT_EQ(engine::brush_weight(brush, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(engine::brush_weight(brush, 2.0f), 1.0f);
    EXPECT_NEAR(engine::brush_weight(brush, 3.0f), 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(engine::brush_weight(brush, 4.0f), 0.0f);
    EXPECT_FLOAT_EQ(engine::brush_weight(brush, 9.0f), 0.0f);
    // it only falls
    float previous = 1.0f;
    for (float distance = 0.0f; distance <= 4.0f; distance += 0.1f)
    {
        const float weight = engine::brush_weight(brush, distance);
        EXPECT_LE(weight, previous + 1e-6f);
        previous = weight;
    }
    // a hard disc, and a brush fading from its center
    EXPECT_FLOAT_EQ(engine::brush_weight({ 4.0f, 1.0f, 0.0f }, 3.99f), 1.0f);
    EXPECT_LT(engine::brush_weight({ 4.0f, 1.0f, 1.0f }, 0.5f), 1.0f);
    EXPECT_FLOAT_EQ(engine::brush_weight({ 0.0f, 1.0f, 0.5f }, 0.0f), 0.0f);
}

TEST(TerrainBrushRegion, TheSamplesWithinTheRadiusCutToTheTerrain)
{
    const auto terrain = flat(33, 2.0f);
    // 3 m around (20, 30): samples 9..11 along x (18..22 m), 14..16 along z
    const auto region = engine::brush_region(terrain, { 20.0f, 30.0f }, 3.0f);
    ASSERT_TRUE(region);
    EXPECT_EQ(region->x, 9u);
    EXPECT_EQ(region->width, 3u);
    EXPECT_EQ(region->z, 14u);
    EXPECT_EQ(region->depth, 3u);
    // over the corner: cut
    const auto corner = engine::brush_region(terrain, { 1.0f, 63.0f }, 3.0f);
    ASSERT_TRUE(corner);
    EXPECT_EQ(corner->x, 0u);
    EXPECT_EQ(corner->width, 3u);
    EXPECT_EQ(corner->z, 30u);
    EXPECT_EQ(corner->depth, 3u);
    // beside the terrain, and between samples
    EXPECT_FALSE(engine::brush_region(terrain, { -10.0f, 10.0f }, 3.0f));
    EXPECT_FALSE(engine::brush_region(terrain, { 80.0f, 10.0f }, 3.0f));
    EXPECT_FALSE(engine::brush_region(terrain, { 21.0f, 31.0f }, 0.5f));
    EXPECT_FALSE(engine::brush_region(terrain, { 20.0f, 30.0f }, 0.0f));
}

TEST(TerrainBrush, RaiseLiftsTheGroundUnderTheBrushAndLowerDigs)
{
    auto terrain = flat();
    const TerrainBrush brush{ 4.0f, 2.0f, 0.5f };
    const auto region = engine::apply_brush(terrain, TerrainBrushMode::eRaise, brush, { 16.0f, 16.0f }, 0.5f);
    ASSERT_TRUE(region);
    // 2 m/s for half a second in the middle, half of it where the weight is a half, nothing outside
    EXPECT_NEAR(terrain.get_height(16, 16), 1.0f, 1e-5f);
    EXPECT_NEAR(terrain.get_height(18, 16), 1.0f, 1e-5f);
    EXPECT_NEAR(terrain.get_height(19, 16), 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(terrain.get_height(20, 16), 0.0f);
    EXPECT_FLOAT_EQ(terrain.get_height(16, 21), 0.0f);
    EXPECT_NEAR(terrain.get_max_height(), 1.0f, 1e-5f);

    engine::apply_brush(terrain, TerrainBrushMode::eLower, brush, { 16.0f, 16.0f }, 1.0f);
    EXPECT_NEAR(terrain.get_height(16, 16), -1.0f, 1e-5f);
    EXPECT_NEAR(terrain.get_height(19, 16), -0.5f, 1e-5f);
}

TEST(TerrainBrush, StrokeDoesNotDependOnTheFrameRate)
{
    const TerrainBrush brush{ 5.0f, 3.0f, 0.7f };
    for (const auto mode : { TerrainBrushMode::eRaise, TerrainBrushMode::eLower, TerrainBrushMode::eFlatten })
    {
        auto once = slope();
        auto many = slope();
        engine::apply_brush(once, mode, brush, { 8.0f, 8.0f }, 0.4f, 20.0f);
        for (int frame = 0; frame < 40; frame++)
        {
            engine::apply_brush(many, mode, brush, { 8.0f, 8.0f }, 0.01f, 20.0f);
        }
        for (std::size_t i = 0; i < once.get_heights().size(); i++)
        {
            ASSERT_NEAR(once.get_heights()[i], many.get_heights()[i], 1e-3f) << "mode " << static_cast<int>(mode) << " sample " << i;
        }
    }
}

TEST(TerrainBrush, FlattenApproachesItsHeightAndNeverPassesIt)
{
    auto terrain = slope();
    const TerrainBrush brush{ 3.0f, 4.0f, 0.0f };
    // the ground at x = 6..10 is 6..10 m high: towards 8 m
    engine::apply_brush(terrain, TerrainBrushMode::eFlatten, brush, { 8.0f, 8.0f }, 0.1f, 8.0f);
    EXPECT_GT(terrain.get_height(6, 8), 6.0f);
    EXPECT_LT(terrain.get_height(6, 8), 8.0f);
    EXPECT_LT(terrain.get_height(10, 8), 10.0f);
    EXPECT_GT(terrain.get_height(10, 8), 8.0f);
    EXPECT_FLOAT_EQ(terrain.get_height(8, 8), 8.0f);
    // held long enough: flat at that height, the rest untouched
    for (int frame = 0; frame < 300; frame++)
    {
        engine::apply_brush(terrain, TerrainBrushMode::eFlatten, brush, { 8.0f, 8.0f }, 1.0f / 60.0f, 8.0f);
    }
    EXPECT_NEAR(terrain.get_height(6, 8), 8.0f, 1e-3f);
    EXPECT_NEAR(terrain.get_height(10, 8), 8.0f, 1e-3f);
    EXPECT_NEAR(terrain.get_height(8, 10), 8.0f, 1e-3f);
    EXPECT_FLOAT_EQ(terrain.get_height(12, 8), 12.0f);
}

TEST(TerrainBrush, SmoothTakesOutASpikeAndLeavesAPlaneAlone)
{
    auto spike = flat();
    spike.set_heights({ 16, 16, 1, 1 }, std::vector<float>{ 9.0f });
    const TerrainBrush brush{ 4.0f, 5.0f, 0.0f };
    engine::apply_brush(spike, TerrainBrushMode::eSmooth, brush, { 16.0f, 16.0f }, 0.1f);
    const float after_one = spike.get_height(16, 16);
    EXPECT_LT(after_one, 9.0f);
    EXPECT_GT(after_one, 1.0f);
    // its neighbours take some of it
    EXPECT_GT(spike.get_height(15, 16), 0.0f);
    for (int frame = 0; frame < 200; frame++)
    {
        engine::apply_brush(spike, TerrainBrushMode::eSmooth, brush, { 16.0f, 16.0f }, 1.0f / 60.0f);
    }
    EXPECT_LT(spike.get_max_height(), 0.5f);

    // a plane is its own average (away from the terrain's edges)
    auto plane = slope();
    const auto before = all_heights(plane);
    engine::apply_brush(plane, TerrainBrushMode::eSmooth, brush, { 8.0f, 8.0f }, 1.0f);
    for (std::size_t i = 0; i < before.size(); i++)
    {
        EXPECT_NEAR(plane.get_heights()[i], before[i], 1e-4f) << i;
    }
}

TEST(TerrainBrush, NothingChangesBesideTheTerrainOrWithoutTime)
{
    auto terrain = flat();
    const auto revision = terrain.get_revision();
    const TerrainBrush brush{ 4.0f, 2.0f, 0.5f };
    EXPECT_FALSE(engine::apply_brush(terrain, TerrainBrushMode::eRaise, brush, { -20.0f, 5.0f }, 1.0f));
    EXPECT_FALSE(engine::apply_brush(terrain, TerrainBrushMode::eRaise, brush, { 16.0f, 16.0f }, 0.0f));
    EXPECT_EQ(terrain.get_revision(), revision);
    EXPECT_FLOAT_EQ(terrain.get_max_height(), 0.0f);
}

// -- a stroke and its undo step --------------------------------------------------------------------------------------

namespace
{
// terrains by name, like the editor's
class FakeTerrains : public engine::ITerrainHeights
{
public:
    bool set_terrain_heights(std::string_view terrain, const TerrainData::Region& region, std::span<const float> heights) override
    {
        const auto it = terrains.find(std::string(terrain));
        return it != terrains.end() && it->second.set_heights(region, heights);
    }

    std::map<std::string, TerrainData> terrains;
};

engine::Scene& no_scene()
{
    // the stroke's step never touches the scene it is given
    static int storage = 0;
    return *reinterpret_cast<engine::Scene*>(&storage);
}
}  // namespace

TEST(TerrainStroke, RecordsTheHeightsBeforeTheStrokeOverEverythingItTouched)
{
    auto terrain = slope();
    const auto original = all_heights(terrain);
    const TerrainBrush brush{ 2.0f, 3.0f, 0.5f };
    engine::TerrainStroke stroke;
    EXPECT_TRUE(stroke.is_empty());
    // dragged along x, overlapping steps
    for (const float x : { 4.0f, 5.0f, 6.5f, 9.0f })
    {
        const auto region = engine::brush_region(terrain, { x, 8.0f }, brush.radius);
        ASSERT_TRUE(region);
        stroke.include(terrain, *region);
        engine::apply_brush(terrain, TerrainBrushMode::eRaise, brush, { x, 8.0f }, 0.1f);
    }
    ASSERT_FALSE(stroke.is_empty());
    const auto& region = stroke.get_region();
    EXPECT_EQ(region.x, 2u);
    EXPECT_EQ(region.width, 10u);
    EXPECT_EQ(region.z, 6u);
    EXPECT_EQ(region.depth, 5u);
    // what it recorded is the terrain before the stroke, not after its earlier steps
    std::vector<float> expected;
    for (std::uint32_t z = region.z; z < region.z + region.depth; z++)
    {
        for (std::uint32_t x = region.x; x < region.x + region.width; x++)
        {
            expected.push_back(original[z * 17 + x]);
        }
    }
    EXPECT_EQ(stroke.get_before(), expected);
}

TEST(TerrainStroke, UndoGivesTheHeightsBackExactlyAndRedoTheStroke)
{
    FakeTerrains terrains;
    terrains.terrains.emplace("valley.terrain", slope());
    auto& terrain = terrains.terrains.at("valley.terrain");
    const auto original = all_heights(terrain);
    const TerrainBrush brush{ 3.0f, 2.0f, 0.5f };
    engine::TerrainStroke stroke;
    for (const auto& center : { glm::vec2(5.0f, 5.0f), glm::vec2(7.0f, 6.0f), glm::vec2(12.0f, 14.0f) })
    {
        stroke.include(terrain, *engine::brush_region(terrain, center, brush.radius));
        engine::apply_brush(terrain, TerrainBrushMode::eSmooth, brush, center, 0.2f);
        engine::apply_brush(terrain, TerrainBrushMode::eRaise, brush, center, 0.2f);
    }
    const auto sculpted = all_heights(terrain);
    ASSERT_NE(sculpted, original);
    std::vector<float> after(stroke.get_before().size());
    ASSERT_TRUE(terrain.get_heights(stroke.get_region(), after));

    engine::CommandManager commands;
    commands.execute_command(no_scene(), std::make_unique<engine::CommandTerrainStroke>(terrains, "valley.terrain", stroke.get_region(), stroke.get_before(), after));
    EXPECT_EQ(all_heights(terrain), sculpted);
    // a terrain is a file of its own: the scene isn't unsaved by the stroke
    EXPECT_FALSE(commands.is_dirty());

    commands.undo(no_scene());
    EXPECT_EQ(all_heights(terrain), original);
    commands.redo(no_scene());
    EXPECT_EQ(all_heights(terrain), sculpted);
}

TEST(TerrainStroke, StepStaysWhenTheSceneIsReleasedAndAfterStop)
{
    FakeTerrains terrains;
    terrains.terrains.emplace("valley.terrain", flat(9));
    auto& terrain = terrains.terrains.at("valley.terrain");
    const auto stroke_step = [&terrains] {
        return std::make_unique<engine::CommandTerrainStroke>(terrains, "valley.terrain", TerrainData::Region{ 2, 2, 1, 1 }, std::vector<float>{ 0.0f },
            std::vector<float>{ 3.0f });
    };
    engine::CommandManager commands;
    commands.execute_command(no_scene(), stroke_step());
    ASSERT_FLOAT_EQ(terrain.get_height(2, 2), 3.0f);
    // another scene is opened: the terrain keeps its change, so its step can still be undone
    commands.clear();
    commands.undo(no_scene());
    EXPECT_FLOAT_EQ(terrain.get_height(2, 2), 0.0f);

    // the same over a play session's Stop
    commands.execute_command(no_scene(), stroke_step());
    commands.begin_play_session();
    commands.end_play_session();
    commands.undo(no_scene());
    EXPECT_FLOAT_EQ(terrain.get_height(2, 2), 0.0f);
}

TEST(TerrainStroke, StrokeWhichChangedNothingOrLostItsTerrainLeavesNoStep)
{
    FakeTerrains terrains;
    terrains.terrains.emplace("valley.terrain", flat(9));
    engine::CommandManager commands;
    // the same heights before and after (the brush was held beside the terrain)
    commands.execute_command(no_scene(), std::make_unique<engine::CommandTerrainStroke>(terrains, "valley.terrain", TerrainData::Region{ 2, 2, 1, 1 },
        std::vector<float>{ 0.0f }, std::vector<float>{ 0.0f }));
    EXPECT_FALSE(commands.can_undo());
    // a terrain which isn't there
    commands.execute_command(no_scene(), std::make_unique<engine::CommandTerrainStroke>(terrains, "gone.terrain", TerrainData::Region{ 2, 2, 1, 1 },
        std::vector<float>{ 0.0f }, std::vector<float>{ 1.0f }));
    EXPECT_FALSE(commands.can_undo());
    // a region which doesn't fit the terrain any more (it was imported again, smaller)
    test_logger::messages().clear();
    commands.execute_command(no_scene(), std::make_unique<engine::CommandTerrainStroke>(terrains, "valley.terrain", TerrainData::Region{ 8, 8, 2, 2 },
        std::vector<float>(4, 0.0f), std::vector<float>(4, 1.0f)));
    EXPECT_FALSE(commands.can_undo());
    EXPECT_TRUE(test_logger::logged("isn't inside"));
}
