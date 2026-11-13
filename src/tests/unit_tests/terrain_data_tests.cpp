#include "terrain_data.h"
#include "test_logger.h"

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

namespace
{
using engine::TerrainData;

// height = x + 10 * z at every sample
TerrainData ramp(std::uint32_t samples_x, std::uint32_t samples_z, float cell_size = 1.0f)
{
    std::vector<float> heights(static_cast<std::size_t>(samples_x) * samples_z);
    for (std::uint32_t z = 0; z < samples_z; z++)
    {
        for (std::uint32_t x = 0; x < samples_x; x++)
        {
            heights[static_cast<std::size_t>(z) * samples_x + x] = static_cast<float>(x) + 10.0f * static_cast<float>(z);
        }
    }
    return *TerrainData::create(samples_x, samples_z, cell_size, heights);
}
}  // namespace

TEST(TerrainDataTest, NewTerrainIsFlat)
{
    const auto terrain = TerrainData::create(5, 3, 2.0f);
    ASSERT_TRUE(terrain);
    EXPECT_EQ(terrain->get_samples_x(), 5u);
    EXPECT_EQ(terrain->get_samples_z(), 3u);
    EXPECT_EQ(terrain->get_heights().size(), 15u);
    EXPECT_FLOAT_EQ(terrain->get_size().x, 8.0f);
    EXPECT_FLOAT_EQ(terrain->get_size().y, 4.0f);
    EXPECT_FLOAT_EQ(terrain->get_min_height(), 0.0f);
    EXPECT_FLOAT_EQ(terrain->get_max_height(), 0.0f);
    EXPECT_FLOAT_EQ(*terrain->sample_height(3.0f, 1.5f), 0.0f);
}

TEST(TerrainDataTest, RefusesSizesAndHeightsWhichMakeNoTerrain)
{
    test_logger::messages().clear();
    EXPECT_FALSE(TerrainData::create(1, 5, 1.0f));
    EXPECT_FALSE(TerrainData::create(5, engine::TERRAIN_MAX_SAMPLES + 1, 1.0f));
    EXPECT_FALSE(TerrainData::create(5, 5, 0.0f));
    EXPECT_FALSE(TerrainData::create(5, 5, -1.0f));
    const std::vector<float> too_few(24, 0.0f);
    EXPECT_FALSE(TerrainData::create(5, 5, 1.0f, too_few));
    std::vector<float> not_finite(25, 0.0f);
    not_finite[7] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(TerrainData::create(5, 5, 1.0f, not_finite));
    EXPECT_TRUE(test_logger::logged("2 to"));
    EXPECT_TRUE(test_logger::logged("finite"));
}

TEST(TerrainDataTest, HeightsAreRowByRow)
{
    const auto terrain = ramp(4, 3);
    EXPECT_FLOAT_EQ(terrain.get_height(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(terrain.get_height(3, 0), 3.0f);
    EXPECT_FLOAT_EQ(terrain.get_height(1, 2), 21.0f);
    EXPECT_FLOAT_EQ(terrain.get_min_height(), 0.0f);
    EXPECT_FLOAT_EQ(terrain.get_max_height(), 23.0f);
}

TEST(TerrainDataTest, SamplesTheSurfaceBetweenSamples)
{
    // a plane: every point of it is on the surface whichever triangle it falls in
    const auto terrain = ramp(4, 3, 2.0f);
    EXPECT_NEAR(*terrain.sample_height(0.0f, 0.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(*terrain.sample_height(3.0f, 1.0f), 1.5f + 5.0f, 1e-5f);
    EXPECT_NEAR(*terrain.sample_height(5.5f, 3.9f), 2.75f + 19.5f, 1e-4f);
    // the last edges belong to the terrain
    EXPECT_NEAR(*terrain.sample_height(6.0f, 4.0f), 23.0f, 1e-5f);
    EXPECT_FALSE(terrain.sample_height(-0.01f, 1.0f));
    EXPECT_FALSE(terrain.sample_height(1.0f, 4.01f));
    EXPECT_FALSE(terrain.sample_height(6.01f, 1.0f));
    EXPECT_FALSE(terrain.sample_height(std::numeric_limits<float>::quiet_NaN(), 1.0f));
}

TEST(TerrainDataTest, CellsAreSplitAlongTheDiagonalTheOtherSystemsUse)
{
    // one cell with only the corner (1, 1) raised: the diagonal from (0, 1) to (1, 0) stays on the ground
    const std::vector<float> heights = { 0.0f, 0.0f, 0.0f, 1.0f };
    const auto terrain = *TerrainData::create(2, 2, 1.0f, heights);
    EXPECT_NEAR(*terrain.sample_height(0.5f, 0.5f), 0.0f, 1e-6f);
    EXPECT_NEAR(*terrain.sample_height(0.25f, 0.25f), 0.0f, 1e-6f);
    // on the raised triangle the height grows towards the corner
    EXPECT_NEAR(*terrain.sample_height(0.75f, 0.75f), 0.5f, 1e-6f);
    EXPECT_NEAR(*terrain.sample_height(1.0f, 1.0f), 1.0f, 1e-6f);
    // the flat triangle faces up, the raised one leans away from its corner
    const auto flat = *terrain.sample_normal(0.25f, 0.25f);
    EXPECT_NEAR(flat.y, 1.0f, 1e-6f);
    const auto raised = *terrain.sample_normal(0.75f, 0.75f);
    EXPECT_LT(raised.x, -0.1f);
    EXPECT_LT(raised.z, -0.1f);
    EXPECT_GT(raised.y, 0.1f);
    EXPECT_NEAR(glm::length(raised), 1.0f, 1e-5f);
}

TEST(TerrainDataTest, NormalOfASlopeLeansDownhill)
{
    // rises 1 per metre along x
    const auto terrain = ramp(3, 2);
    const auto normal = *terrain.sample_normal(0.5f, 0.25f);
    EXPECT_NEAR(normal.x, -normal.y * 1.0f, 1e-5f);
    EXPECT_LT(normal.x, 0.0f);
}

TEST(TerrainDataTest, ReadsAndReplacesARegion)
{
    auto terrain = *TerrainData::create(6, 5, 1.0f);
    const auto revision = terrain.get_revision();
    const std::vector<float> patch = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    ASSERT_TRUE(terrain.set_heights({ 2, 1, 3, 2 }, patch));
    EXPECT_GT(terrain.get_revision(), revision);
    EXPECT_FLOAT_EQ(terrain.get_height(2, 1), 1.0f);
    EXPECT_FLOAT_EQ(terrain.get_height(4, 1), 3.0f);
    EXPECT_FLOAT_EQ(terrain.get_height(2, 2), 4.0f);
    EXPECT_FLOAT_EQ(terrain.get_height(4, 2), 6.0f);
    // around it nothing changed
    EXPECT_FLOAT_EQ(terrain.get_height(1, 1), 0.0f);
    EXPECT_FLOAT_EQ(terrain.get_height(5, 2), 0.0f);
    EXPECT_FLOAT_EQ(terrain.get_height(2, 3), 0.0f);
    EXPECT_FLOAT_EQ(terrain.get_max_height(), 6.0f);

    std::vector<float> read(6);
    ASSERT_TRUE(terrain.get_heights({ 2, 1, 3, 2 }, read));
    EXPECT_EQ(read, patch);
}

TEST(TerrainDataTest, RefusesRegionsOutsideTheTerrain)
{
    auto terrain = *TerrainData::create(6, 5, 1.0f);
    const auto revision = terrain.get_revision();
    const std::vector<float> patch(6, 1.0f);
    test_logger::messages().clear();
    EXPECT_FALSE(terrain.set_heights({ 4, 1, 3, 2 }, patch));
    EXPECT_FALSE(terrain.set_heights({ 2, 4, 3, 2 }, patch));
    EXPECT_FALSE(terrain.set_heights({ 6, 0, 1, 1 }, std::vector<float>(1, 1.0f)));
    EXPECT_FALSE(terrain.set_heights({ 0, 0, 0, 0 }, {}));
    // a width which would wrap around
    EXPECT_FALSE(terrain.set_heights({ 2, 1, std::numeric_limits<std::uint32_t>::max(), 1 }, patch));
    // the wrong number of heights
    EXPECT_FALSE(terrain.set_heights({ 0, 0, 3, 3 }, patch));
    EXPECT_TRUE(test_logger::logged("isn't inside"));
    EXPECT_EQ(terrain.get_revision(), revision);
    EXPECT_FLOAT_EQ(terrain.get_max_height(), 0.0f);

    std::vector<float> read(6);
    EXPECT_FALSE(terrain.get_heights({ 4, 1, 3, 2 }, read));
    EXPECT_FALSE(terrain.get_heights({ 0, 0, 2, 2 }, read));
}

TEST(TerrainDataTest, ChunksCoverTheTerrainAndShareTheirEdges)
{
    // 70 cells along x: chunks of 32, 32 and 6 cells; 32 along z: one chunk
    const auto terrain = *TerrainData::create(71, 33, 0.5f);
    EXPECT_EQ(terrain.get_chunks_x(), 3u);
    EXPECT_EQ(terrain.get_chunks_z(), 1u);
    ASSERT_EQ(terrain.get_chunks().size(), 3u);
    const auto& chunks = terrain.get_chunks();
    EXPECT_EQ(chunks[1].x, 32u);
    EXPECT_FLOAT_EQ(chunks[0].bounds.min.x, 0.0f);
    EXPECT_FLOAT_EQ(chunks[0].bounds.max.x, 16.0f);
    EXPECT_FLOAT_EQ(chunks[1].bounds.min.x, 16.0f);
    EXPECT_FLOAT_EQ(chunks[1].bounds.max.x, 32.0f);
    // the last chunk ends with the terrain
    EXPECT_FLOAT_EQ(chunks[2].bounds.min.x, 32.0f);
    EXPECT_FLOAT_EQ(chunks[2].bounds.max.x, 35.0f);
    EXPECT_FLOAT_EQ(chunks[2].bounds.max.z, 16.0f);
    const auto bounds = terrain.get_bounds();
    EXPECT_FLOAT_EQ(bounds.max.x, 35.0f);
    EXPECT_FLOAT_EQ(bounds.max.z, 16.0f);
}

TEST(TerrainDataTest, ChangedHeightsUpdateTheBoundsOfTheChunksHoldingThem)
{
    auto terrain = *TerrainData::create(97, 97, 1.0f);
    ASSERT_EQ(terrain.get_chunks().size(), 9u);
    // inside the middle chunk
    ASSERT_TRUE(terrain.set_heights({ 40, 40, 1, 1 }, std::vector<float>{ 5.0f }));
    for (std::size_t i = 0; i < 9; i++)
    {
        EXPECT_FLOAT_EQ(terrain.get_chunks()[i].bounds.max.y, i == 4 ? 5.0f : 0.0f) << "chunk " << i;
    }
    // a sample on the corner of four chunks is in all of them
    ASSERT_TRUE(terrain.set_heights({ 32, 32, 1, 1 }, std::vector<float>{ -3.0f }));
    for (const std::size_t i : { 0u, 1u, 3u, 4u })
    {
        EXPECT_FLOAT_EQ(terrain.get_chunks()[i].bounds.min.y, -3.0f) << "chunk " << i;
    }
    for (const std::size_t i : { 2u, 5u, 6u, 7u, 8u })
    {
        EXPECT_FLOAT_EQ(terrain.get_chunks()[i].bounds.min.y, 0.0f) << "chunk " << i;
    }
    EXPECT_FLOAT_EQ(terrain.get_min_height(), -3.0f);
    EXPECT_FLOAT_EQ(terrain.get_max_height(), 5.0f);
    // lowering the peak again shrinks the bounds
    ASSERT_TRUE(terrain.set_heights({ 40, 40, 1, 1 }, std::vector<float>{ 0.0f }));
    EXPECT_FLOAT_EQ(terrain.get_chunks()[4].bounds.max.y, 0.0f);
    EXPECT_FLOAT_EQ(terrain.get_max_height(), 0.0f);
}

TEST(TerrainDataTest, ReportsTheRowsChangedSinceTheyWereTaken)
{
    auto terrain = *TerrainData::create(8, 16, 1.0f);
    // a new terrain: every row
    const auto all = terrain.take_changed_rows();
    ASSERT_TRUE(all);
    EXPECT_EQ(all->first, 0u);
    EXPECT_EQ(all->second, 15u);
    EXPECT_FALSE(terrain.take_changed_rows());

    ASSERT_TRUE(terrain.set_heights({ 1, 9, 2, 2 }, std::vector<float>(4, 1.0f)));
    ASSERT_TRUE(terrain.set_heights({ 0, 4, 1, 1 }, std::vector<float>(1, 1.0f)));
    const auto rows = terrain.take_changed_rows();
    ASSERT_TRUE(rows);
    EXPECT_EQ(rows->first, 4u);
    EXPECT_EQ(rows->second, 10u);
    EXPECT_FALSE(terrain.take_changed_rows());
}

TEST(TerrainDataTest, ChangeCountMovesWithEveryChangeAndOnceForABatch)
{
    auto terrain = std::make_shared<TerrainData>(*TerrainData::create(9, 9, 1.0f));
    const std::vector<float> one{ 1.0f };
    auto count = TerrainData::get_change_count();
    ASSERT_TRUE(terrain->set_heights({ 1, 1, 1, 1 }, one));
    EXPECT_GT(TerrainData::get_change_count(), count);

    // a batch: the heights and their revision change, whoever follows the terrains hears of it once, at its end
    count = TerrainData::get_change_count();
    const auto revision = terrain->get_revision();
    {
        const TerrainData::EditBatch batch(terrain);
        ASSERT_TRUE(terrain->set_heights({ 2, 2, 1, 1 }, one));
        ASSERT_TRUE(terrain->set_heights({ 3, 3, 1, 1 }, one));
        EXPECT_EQ(TerrainData::get_change_count(), count);
        EXPECT_GT(terrain->get_revision(), revision);
        EXPECT_FLOAT_EQ(terrain->get_height(3, 3), 1.0f);
        // one inside another ends with the outer one
        {
            const TerrainData::EditBatch inner(terrain);
            ASSERT_TRUE(terrain->set_heights({ 4, 4, 1, 1 }, one));
        }
        EXPECT_EQ(TerrainData::get_change_count(), count);
    }
    EXPECT_GT(TerrainData::get_change_count(), count);
    // and changes tell at once again
    count = TerrainData::get_change_count();
    ASSERT_TRUE(terrain->set_heights({ 5, 5, 1, 1 }, one));
    EXPECT_GT(TerrainData::get_change_count(), count);
}

TEST(TerrainDataTest, FileRoundTrip)
{
    const auto terrain = ramp(7, 4, 0.25f);
    const auto content = terrain.serialize();
    const auto loaded = TerrainData::parse(content, "test");
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->get_samples_x(), 7u);
    EXPECT_EQ(loaded->get_samples_z(), 4u);
    EXPECT_FLOAT_EQ(loaded->get_cell_size(), 0.25f);
    ASSERT_EQ(loaded->get_heights().size(), terrain.get_heights().size());
    EXPECT_TRUE(std::equal(loaded->get_heights().begin(), loaded->get_heights().end(), terrain.get_heights().begin()));
    EXPECT_FLOAT_EQ(loaded->get_max_height(), terrain.get_max_height());
    // the same terrain gives the same bytes
    EXPECT_EQ(loaded->serialize(), content);
}

TEST(TerrainDataTest, RefusesFilesWhichAreNotTerrains)
{
    const auto content = ramp(7, 4).serialize();
    test_logger::messages().clear();
    EXPECT_FALSE(TerrainData::parse("", "empty.terrain"));
    EXPECT_FALSE(TerrainData::parse("{ \"version\": 1 } and enough bytes to hold a header", "scene.json"));
    EXPECT_TRUE(test_logger::logged("scene.json: not a terrain file"));
    // cut short, and with bytes too many
    EXPECT_FALSE(TerrainData::parse(std::string_view(content).substr(0, content.size() - 4), "short.terrain"));
    EXPECT_FALSE(TerrainData::parse(content + "1234", "long.terrain"));
    // a version this build doesn't know
    auto newer = content;
    const std::uint32_t version = 99;
    std::memcpy(newer.data() + 8, &version, sizeof(version));
    EXPECT_FALSE(TerrainData::parse(newer, "newer.terrain"));
    EXPECT_TRUE(test_logger::logged("version 99"));
    // a size the heights don't match
    auto resized = content;
    const std::uint32_t samples = 8;
    std::memcpy(resized.data() + 12, &samples, sizeof(samples));
    EXPECT_FALSE(TerrainData::parse(resized, "resized.terrain"));
    // a height which isn't a number
    auto broken = content;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::memcpy(broken.data() + 24, &nan, sizeof(nan));
    EXPECT_FALSE(TerrainData::parse(broken, "broken.terrain"));
}

TEST(TerrainDataTest, HeightmapPixelsBecomeHeightsOfTheRange)
{
    const std::vector<std::uint16_t> pixels = { 0, 65535, 32768, 16384, 0, 0 };
    const auto terrain = TerrainData::from_heightmap(pixels, 3, 2, 2.0f, -10.0f, 30.0f);
    ASSERT_TRUE(terrain);
    EXPECT_EQ(terrain->get_samples_x(), 3u);
    EXPECT_EQ(terrain->get_samples_z(), 2u);
    EXPECT_FLOAT_EQ(terrain->get_height(0, 0), -10.0f);
    EXPECT_FLOAT_EQ(terrain->get_height(1, 0), 30.0f);
    EXPECT_NEAR(terrain->get_height(2, 0), 10.0f, 1e-3f);
    // the second row of pixels is z = 1
    EXPECT_NEAR(terrain->get_height(0, 1), 0.0f, 1e-3f);

    test_logger::messages().clear();
    EXPECT_FALSE(TerrainData::from_heightmap(pixels, 3, 3, 2.0f, 0.0f, 1.0f));
    EXPECT_FALSE(TerrainData::from_heightmap(pixels, 3, 2, 2.0f, 5.0f, 1.0f));
    EXPECT_TRUE(test_logger::logged("height range"));
}

TEST(TerrainDataTest, RawHeightmapIsASquareOfLittleEndianPixels)
{
    // 3 x 3 pixels; the second one is 0x0100 = 256
    std::string content(18, '\0');
    content[2] = '\0';
    content[3] = '\1';
    content[16] = '\xff';
    content[17] = '\xff';
    const auto terrain = TerrainData::from_raw16(content, 1.0f, 0.0f, 65535.0f);
    ASSERT_TRUE(terrain);
    EXPECT_EQ(terrain->get_samples_x(), 3u);
    EXPECT_EQ(terrain->get_samples_z(), 3u);
    EXPECT_FLOAT_EQ(terrain->get_height(1, 0), 256.0f);
    EXPECT_FLOAT_EQ(terrain->get_height(2, 2), 65535.0f);

    test_logger::messages().clear();
    // not a square, an odd number of bytes, nothing
    EXPECT_FALSE(TerrainData::from_raw16(std::string(16, '\0'), 1.0f, 0.0f, 1.0f));
    EXPECT_FALSE(TerrainData::from_raw16(std::string(17, '\0'), 1.0f, 0.0f, 1.0f));
    EXPECT_FALSE(TerrainData::from_raw16("", 1.0f, 0.0f, 1.0f));
    EXPECT_TRUE(test_logger::logged("square 16-bit raw heightmap"));
}

TEST(TerrainDataTest, TrianglesFaceUpAndFollowTheMatrix)
{
    const auto terrain = ramp(3, 2, 2.0f);
    std::vector<float> vertices = { 9.0f, 9.0f, 9.0f };  // something there already
    std::vector<std::int32_t> triangles = { 0, 0, 0 };
    terrain.append_triangles(glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 0.0f, 0.0f)), vertices, triangles);
    // 6 vertices and 2 cells of 2 triangles appended
    ASSERT_EQ(vertices.size(), 3u + 6u * 3u);
    ASSERT_EQ(triangles.size(), 3u + 4u * 3u);
    // sample (2, 1), the last one: 4 m along x, moved by the matrix
    EXPECT_FLOAT_EQ(vertices[3 + 5 * 3 + 0], 104.0f);
    EXPECT_FLOAT_EQ(vertices[3 + 5 * 3 + 1], 12.0f);
    EXPECT_FLOAT_EQ(vertices[3 + 5 * 3 + 2], 2.0f);
    for (std::size_t t = 3; t < triangles.size(); t += 3)
    {
        const auto vertex = [&](std::int32_t index) { return glm::vec3(vertices[index * 3], vertices[index * 3 + 1], vertices[index * 3 + 2]); };
        // indices point at the appended vertices
        EXPECT_GE(triangles[t], 1);
        const auto a = vertex(triangles[t]);
        const auto b = vertex(triangles[t + 1]);
        const auto c = vertex(triangles[t + 2]);
        // the winding Recast's slope test reads: (b - a) x (c - a) points up
        EXPECT_GT(glm::cross(b - a, c - a).y, 0.0f);
    }
}

TEST(TerrainData, ContentHashFollowsTheHeightsNotTheirHistory)
{
    auto terrain = TerrainData::create(9, 7, 0.5f);
    ASSERT_TRUE(terrain);
    const auto flat = terrain->content_hash();
    // the same content made again has the same hash
    EXPECT_EQ(TerrainData::create(9, 7, 0.5f)->content_hash(), flat);
    // another size, cell size or height has another one
    EXPECT_NE(TerrainData::create(7, 9, 0.5f)->content_hash(), flat);
    EXPECT_NE(TerrainData::create(9, 7, 1.0f)->content_hash(), flat);
    const std::vector<float> one{ 0.25f };
    ASSERT_TRUE(terrain->set_heights({ 8, 6, 1, 1 }, one));
    EXPECT_NE(terrain->content_hash(), flat);
    // put back (an undo): the revision moved on, the content is what it was
    const auto revision = terrain->get_revision();
    const std::vector<float> zero{ 0.0f };
    ASSERT_TRUE(terrain->set_heights({ 8, 6, 1, 1 }, zero));
    EXPECT_NE(terrain->get_revision(), revision);
    EXPECT_EQ(terrain->content_hash(), flat);
}

TEST(TerrainData, ACopyMadeDuringAnEditBatchIsNotInTheBatch)
{
    auto data = std::make_shared<TerrainData>(*TerrainData::create(5, 5, 1.0f));
    const std::vector<float> one{ 1.0f };
    {
        const TerrainData::EditBatch batch(data);
        // the batch holds `data`, not its copies: a copy's changes are seen at once
        TerrainData copy = *data;
        const auto changes = TerrainData::get_change_count();
        ASSERT_TRUE(copy.set_heights({ 1, 1, 1, 1 }, one));
        EXPECT_NE(TerrainData::get_change_count(), changes);
        // and assigning to the batch's terrain leaves it in its batch
        *data = copy;
        const auto after_assign = TerrainData::get_change_count();
        ASSERT_TRUE(data->set_heights({ 2, 2, 1, 1 }, one));
        EXPECT_EQ(TerrainData::get_change_count(), after_assign);
    }
    const auto changes = TerrainData::get_change_count();
    ASSERT_TRUE(data->set_heights({ 3, 3, 1, 1 }, one));
    EXPECT_NE(TerrainData::get_change_count(), changes);
}
