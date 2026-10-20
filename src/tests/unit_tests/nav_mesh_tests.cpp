#include "nav_mesh.h"

#include <DetourNavMesh.h>

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace
{
// a static box collider with the given half extents
engine_collider_component_t box_collider(const glm::vec3& half_extents)
{
    engine_collider_component_t collider{};
    collider.type = ENGINE_COLLIDER_TYPE_BOX;
    collider.collider.box.size[0] = half_extents.x;
    collider.collider.box.size[1] = half_extents.y;
    collider.collider.box.size[2] = half_extents.z;
    return collider;
}

void add_box(engine::NavMeshInput& input, const glm::vec3& center, const glm::vec3& half_extents, engine::NavArea area = engine::NavArea::eBySlope)
{
    input.add_collider(box_collider(half_extents), glm::translate(glm::mat4(1.0f), center), area);
}

// the xz area covered by the nav mesh's polygons
float walkable_area(const engine::NavMesh& nav_mesh)
{
    const auto& mesh = nav_mesh.get_detour_nav_mesh();
    float area = 0.0f;
    for (int t = 0; t < mesh.getMaxTiles(); t++)
    {
        const auto* tile = mesh.getTile(t);
        if (!tile || !tile->header)
        {
            continue;
        }
        for (int p = 0; p < tile->header->polyCount; p++)
        {
            const auto& poly = tile->polys[p];
            float twice_area = 0.0f;
            for (int i = 0; i < poly.vertCount; i++)
            {
                const float* a = &tile->verts[poly.verts[i] * 3];
                const float* b = &tile->verts[poly.verts[(i + 1) % poly.vertCount] * 3];
                twice_area += a[0] * b[2] - b[0] * a[2];
            }
            area += std::abs(twice_area) * 0.5f;
        }
    }
    return area;
}

float path_length(const std::vector<glm::vec3>& path)
{
    float length = 0.0f;
    for (std::size_t i = 1; i < path.size(); i++)
    {
        length += glm::distance(path[i - 1], path[i]);
    }
    return length;
}

constexpr float FLOOR_TOP = 0.1f;
}  // namespace

// a 10 x 10 m floor with a wall from its west edge to x = 3 across the middle: the path from south to north of the wall
// goes around its east end
TEST(NavMesh, PathGoesAroundWall)
{
    engine::NavMeshInput input;
    add_box(input, { 0.0f, 0.0f, 0.0f }, { 5.0f, FLOOR_TOP, 5.0f });
    add_box(input, { -1.0f, FLOOR_TOP + 1.0f, 0.0f }, { 4.0f, 1.0f, 0.25f });
    const auto nav_mesh = engine::NavMesh::build(input, {});
    ASSERT_NE(nav_mesh, nullptr);

    const glm::vec3 start(-2.0f, FLOOR_TOP, -3.0f);
    const glm::vec3 end(-2.0f, FLOOR_TOP, 3.0f);
    const auto path = nav_mesh->find_path(start, end, 32);
    ASSERT_GE(path.size(), 3u);
    EXPECT_NEAR(glm::distance(path.front(), start), 0.0f, 0.1f);
    EXPECT_NEAR(glm::distance(path.back(), end), 0.0f, 0.1f);
    // around the wall's end (x = 3) plus the agent radius
    const auto max_x = std::max_element(path.begin(), path.end(), [](const auto& a, const auto& b) { return a.x < b.x; })->x;
    EXPECT_GT(max_x, 3.0f);
    EXPECT_GT(path_length(path), 2.0f * glm::distance(glm::vec2(-2.0f, -3.0f), glm::vec2(3.0f, 0.0f)) - 0.5f);

    // the path is cut off after max_points, keeping its beginning
    const auto first_two = nav_mesh->find_path(start, end, 2);
    ASSERT_EQ(first_two.size(), 2u);
    EXPECT_EQ(first_two[1], path[1]);
}

TEST(NavMesh, NoPathToDisconnectedIsland)
{
    engine::NavMeshInput input;
    add_box(input, { 0.0f, 0.0f, 0.0f }, { 3.0f, FLOOR_TOP, 3.0f });
    add_box(input, { 10.0f, 0.0f, 0.0f }, { 3.0f, FLOOR_TOP, 3.0f });
    const auto nav_mesh = engine::NavMesh::build(input, {});
    ASSERT_NE(nav_mesh, nullptr);

    const glm::vec3 on_island(10.0f, FLOOR_TOP, 0.0f);
    EXPECT_TRUE(nav_mesh->find_nearest_point(on_island).has_value());
    EXPECT_TRUE(nav_mesh->find_path({ 0.0f, FLOOR_TOP, 0.0f }, on_island, 32).empty());
    EXPECT_FALSE(nav_mesh->find_path({ 0.0f, FLOOR_TOP, 0.0f }, { 1.0f, FLOOR_TOP, 1.0f }, 32).empty());
    // far from both floors
    EXPECT_FALSE(nav_mesh->find_nearest_point({ 5.0f, FLOOR_TOP, 0.0f }).has_value());
}

// Recast voxelizes all input into one heightfield, so a floor of 1 m tiles gives the same nav mesh as one big box
TEST(NavMesh, TileGranularityDoesNotMatter)
{
    engine::NavMeshInput tiles;
    for (int x = 0; x < 10; x++)
    {
        for (int z = 0; z < 10; z++)
        {
            add_box(tiles, { static_cast<float>(x), 0.0f, static_cast<float>(z) }, { 0.5f, FLOOR_TOP, 0.5f });
        }
    }
    engine::NavMeshInput single_box;
    add_box(single_box, { 4.5f, 0.0f, 4.5f }, { 5.0f, FLOOR_TOP, 5.0f });

    const auto tiles_nav_mesh = engine::NavMesh::build(tiles, {});
    const auto single_box_nav_mesh = engine::NavMesh::build(single_box, {});
    ASSERT_NE(tiles_nav_mesh, nullptr);
    ASSERT_NE(single_box_nav_mesh, nullptr);

    const auto tiles_area = walkable_area(*tiles_nav_mesh);
    const auto single_box_area = walkable_area(*single_box_nav_mesh);
    // 10 x 10 m shrunk by the agent radius on every side
    EXPECT_GT(single_box_area, 80.0f);
    EXPECT_NEAR(tiles_area, single_box_area, single_box_area * 0.01f);
}

TEST(NavMesh, NotWalkableColliderIsLeftOut)
{
    engine::NavMeshInput input;
    add_box(input, { 0.0f, 0.0f, 0.0f }, { 3.0f, FLOOR_TOP, 3.0f });
    add_box(input, { 6.0f, 0.0f, 0.0f }, { 3.0f, FLOOR_TOP, 3.0f }, engine::NavArea::eNotWalkable);
    const auto nav_mesh = engine::NavMesh::build(input, {});
    ASSERT_NE(nav_mesh, nullptr);

    EXPECT_TRUE(nav_mesh->find_nearest_point({ 0.0f, FLOOR_TOP, 0.0f }).has_value());
    EXPECT_FALSE(nav_mesh->find_nearest_point({ 7.0f, FLOOR_TOP, 0.0f }).has_value());
}

TEST(NavMesh, NothingWalkableFailsToBuild)
{
    engine::NavMeshInput empty;
    EXPECT_EQ(engine::NavMesh::build(empty, {}), nullptr);

    engine::NavMeshInput not_walkable;
    add_box(not_walkable, { 0.0f, 0.0f, 0.0f }, { 3.0f, FLOOR_TOP, 3.0f }, engine::NavArea::eNotWalkable);
    EXPECT_EQ(engine::NavMesh::build(not_walkable, {}), nullptr);
}
