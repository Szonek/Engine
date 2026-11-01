#include "debug_draw.h"

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
void expect_vec3_near(const glm::vec3& actual, const glm::vec3& expected)
{
    EXPECT_NEAR(actual.x, expected.x, 1e-4f);
    EXPECT_NEAR(actual.y, expected.y, 1e-4f);
    EXPECT_NEAR(actual.z, expected.z, 1e-4f);
}

bool has_point(const std::vector<engine::DebugDrawLine>& lines, const glm::vec3& point)
{
    return std::ranges::any_of(lines, [&](const engine::DebugDrawLine& line)
        {
            return glm::length(line.from - point) < 1e-4f || glm::length(line.to - point) < 1e-4f;
        });
}
}  // namespace

TEST(DebugDrawList, LineIsKeptUntilTaken)
{
    engine::DebugDrawList list;
    list.add_line(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f, 5.0f, 6.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    ASSERT_EQ(list.lines().size(), 1u);
    expect_vec3_near(list.lines()[0].from, glm::vec3(1.0f, 2.0f, 3.0f));
    expect_vec3_near(list.lines()[0].to, glm::vec3(4.0f, 5.0f, 6.0f));
    expect_vec3_near(list.lines()[0].color, glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST(DebugDrawList, TakeEmptiesTheListForTheNextFrame)
{
    engine::DebugDrawList list;
    list.add_line(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(1.0f));
    list.add_line(glm::vec3(0.0f), glm::vec3(2.0f), glm::vec3(1.0f));

    const auto frame = list.take_lines();
    EXPECT_EQ(frame.size(), 2u);
    EXPECT_TRUE(list.lines().empty());
    EXPECT_TRUE(list.take_lines().empty());

    list.add_line(glm::vec3(0.0f), glm::vec3(3.0f), glm::vec3(1.0f));
    EXPECT_EQ(list.take_lines().size(), 1u);
}

TEST(DebugDrawList, BoxIsItsTwelveEdgesInTheSpaceOfItsTransform)
{
    engine::DebugDrawList list;
    const auto transform = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f))
        * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    list.add_box(transform, glm::vec3(2.0f, 1.0f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f));

    const auto& lines = list.lines();
    ASSERT_EQ(lines.size(), 12u);
    // every edge is parallel to one of the box's axes, so as long as one of its half extents doubled
    for (const auto& line : lines)
    {
        const auto length = glm::length(line.to - line.from);
        EXPECT_TRUE(std::abs(length - 4.0f) < 1e-4f || std::abs(length - 2.0f) < 1e-4f || std::abs(length - 1.0f) < 1e-4f) << length;
    }
    // rotated 90 degrees around y, the box's x extent lies along -z: (2, 1, 0.5) goes to (10.5, 1, -2)
    EXPECT_TRUE(has_point(lines, glm::vec3(10.5f, 1.0f, -2.0f)));
    EXPECT_TRUE(has_point(lines, glm::vec3(9.5f, -1.0f, 2.0f)));
    EXPECT_FALSE(has_point(lines, glm::vec3(12.0f, 1.0f, 0.5f)));
}

TEST(DebugDrawList, CircleLiesInThePlaneOfItsNormalAtItsRadius)
{
    engine::DebugDrawList list;
    const glm::vec3 center(1.0f, 2.0f, 3.0f);
    const glm::vec3 normal(0.0f, 0.0f, 5.0f);
    list.add_circle(center, normal, 2.0f, glm::vec3(1.0f));

    const auto& lines = list.lines();
    ASSERT_EQ(lines.size(), engine::DebugDrawList::CIRCLE_SEGMENTS);
    for (std::size_t i = 0; i < lines.size(); i++)
    {
        EXPECT_NEAR(glm::length(lines[i].from - center), 2.0f, 1e-4f);
        EXPECT_NEAR(lines[i].from.z, center.z, 1e-4f);
        // closed: every segment starts where the one before ended
        expect_vec3_near(lines[i].from, lines[(i + lines.size() - 1) % lines.size()].to);
    }
}

TEST(DebugDrawList, SphereIsThreeCirclesAroundTheAxesAtItsRadius)
{
    engine::DebugDrawList list;
    const glm::vec3 center(1.0f, 2.0f, 3.0f);
    list.add_sphere(center, 0.5f, glm::vec3(1.0f));

    const auto& lines = list.lines();
    ASSERT_EQ(lines.size(), 3 * engine::DebugDrawList::CIRCLE_SEGMENTS);
    for (const auto& line : lines)
    {
        EXPECT_NEAR(glm::length(line.from - center), 0.5f, 1e-4f);
    }
    // the points where the axes leave the sphere are on it
    EXPECT_TRUE(has_point(lines, center + glm::vec3(0.0f, 0.5f, 0.0f)) || has_point(lines, center + glm::vec3(0.0f, -0.5f, 0.0f)));
}

TEST(DebugDrawList, CircleWithoutANormalDrawsNothing)
{
    engine::DebugDrawList list;
    list.add_circle(glm::vec3(0.0f), glm::vec3(0.0f), 1.0f, glm::vec3(1.0f));
    EXPECT_TRUE(list.lines().empty());
}

TEST(DebugDrawList, ShapesDrawnAfterAnOwnerBelongToIt)
{
    engine::DebugDrawList list;
    list.add_line(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(1.0f));
    list.set_owner(7);
    list.add_sphere(glm::vec3(0.0f), 1.0f, glm::vec3(1.0f));
    list.set_owner(3);
    list.add_line(glm::vec3(0.0f), glm::vec3(2.0f), glm::vec3(1.0f));
    list.set_owner(0);
    list.add_line(glm::vec3(0.0f), glm::vec3(3.0f), glm::vec3(1.0f));

    const auto lines = list.take_lines();
    EXPECT_EQ(lines.front().owner, 0u);
    EXPECT_EQ(lines[1].owner, 7u);
    EXPECT_EQ(lines[lines.size() - 2].owner, 3u);
    EXPECT_EQ(lines.back().owner, 0u);
    // what is on the screen now, each owner once
    EXPECT_EQ(list.last_owners(), (std::vector<std::uint32_t>{ 3u, 7u }));
}

TEST(DebugDrawList, TheOwnerEndsWithTheFrame)
{
    engine::DebugDrawList list;
    list.set_owner(5);
    list.add_line(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(1.0f));
    list.take_lines();

    list.add_line(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(1.0f));
    EXPECT_EQ(list.take_lines().front().owner, 0u);
    EXPECT_TRUE(list.last_owners().empty());
}

TEST(PickGameObject, TheNearestGizmoLineNearTheClickWins)
{
    // 5 x 5 pixels around the click at (2, 2): a mesh (1) everywhere, a gizmo of 9 one pixel away, one of 8 further
    std::vector<std::uint32_t> ids(25, 1u);
    ids[2 * 5 + 3] = 9u;
    ids[0 * 5 + 0] = 8u;
    const std::vector<std::uint32_t> owners{ 8u, 9u };
    EXPECT_EQ(engine::pick_game_object(ids, 5, 5, 2, 2, owners), 9u);
}

TEST(PickGameObject, WithoutAGizmoNearTheClickThePixelUnderItIsPicked)
{
    std::vector<std::uint32_t> ids(9, 0u);
    ids[1 * 3 + 1] = 4u;
    // 6 is a mesh, not a gizmo owner: it doesn't win from a distance
    ids[0] = 6u;
    EXPECT_EQ(engine::pick_game_object(ids, 3, 3, 1, 1, std::vector<std::uint32_t>{ 2u }), 4u);
    // empty space under the click
    ids[1 * 3 + 1] = 0u;
    EXPECT_EQ(engine::pick_game_object(ids, 3, 3, 1, 1, std::vector<std::uint32_t>{ 2u }), 0u);
}
