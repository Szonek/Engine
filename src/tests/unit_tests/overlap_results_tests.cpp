#include "overlap_results.h"

#include <gtest/gtest.h>

#include <array>
#include <vector>

using engine::OverlapHit;
using engine::write_nearest_first;

TEST(OverlapResults, NearestFirst)
{
    std::vector<OverlapHit> hits = { { 4, 2.0f }, { 7, 0.5f }, { 2, 1.0f } };
    std::array<engine_game_object_t, 3> out{};
    EXPECT_EQ(write_nearest_first(hits, out), 3u);
    EXPECT_EQ(out, (std::array<engine_game_object_t, 3>{ 7, 2, 4 }));
}

TEST(OverlapResults, InsideIsNearerThanOutside)
{
    // a negative distance: the center is inside that collider
    std::vector<OverlapHit> hits = { { 3, 0.2f }, { 9, -0.4f } };
    std::array<engine_game_object_t, 2> out{};
    write_nearest_first(hits, out);
    EXPECT_EQ(out[0], 9u);
}

TEST(OverlapResults, TiesAreOrderedByHandle)
{
    std::vector<OverlapHit> hits = { { 8, 1.0f }, { 3, 1.0f }, { 5, 1.0f } };
    std::array<engine_game_object_t, 3> out{};
    write_nearest_first(hits, out);
    EXPECT_EQ(out, (std::array<engine_game_object_t, 3>{ 3, 5, 8 }));
}

TEST(OverlapResults, AFullBufferKeepsTheNearestAndCountsAll)
{
    std::vector<OverlapHit> hits = { { 4, 2.0f }, { 7, 0.5f }, { 2, 1.0f } };
    std::array<engine_game_object_t, 2> out = { ENGINE_INVALID_GAME_OBJECT_ID, ENGINE_INVALID_GAME_OBJECT_ID };
    EXPECT_EQ(write_nearest_first(hits, out), 3u);
    EXPECT_EQ(out, (std::array<engine_game_object_t, 2>{ 7, 2 }));
}

TEST(OverlapResults, NoBufferOnlyCounts)
{
    std::vector<OverlapHit> hits = { { 4, 2.0f }, { 7, 0.5f } };
    EXPECT_EQ(write_nearest_first(hits, {}), 2u);
}

TEST(OverlapResults, NothingFound)
{
    std::vector<OverlapHit> hits;
    std::array<engine_game_object_t, 2> out = { 11, 12 };
    EXPECT_EQ(write_nearest_first(hits, out), 0u);
    // nothing is written
    EXPECT_EQ(out, (std::array<engine_game_object_t, 2>{ 11, 12 }));
}
