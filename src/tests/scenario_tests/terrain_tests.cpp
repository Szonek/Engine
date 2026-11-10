#include "scenario_harness.h"

#include <iscene.h>

#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Heightmap terrains: the terrain asset (made by code, imported, saved and loaded), the heightfield collider (ray
// casts, dynamic bodies, the character controller), the nav mesh built from it and scene files referencing it.
namespace
{
class TerrainScene : public engine::IScene
{
public:
    TerrainScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "scenario_terrain"; }
};

glm::vec3 position_of(engine_game_object_t go)
{
    const auto tc = engineGameObjectGetTransformComponent(go);
    return { tc.position[0], tc.position[1], tc.position[2] };
}

class TerrainTest : public scenario::ScenarioTest
{
protected:
    void SetUp() override
    {
        load_scene<TerrainScene>();
    }

    void TearDown() override
    {
        for (const auto terrain : terrains_)
        {
            engineTerrainRelease(terrain);
        }
        for (const auto& file : files_)
        {
            std::error_code ignored;
            std::filesystem::remove(file, ignored);
        }
        ScenarioTest::TearDown();
    }

    // throws the scene away and starts with a new, empty one
    void new_scene()
    {
        scenario::app().unregister_scene(TerrainScene::get_name());
        load_scene<TerrainScene>();
    }

    // a terrain asset whose height at sample (x, z) is height(x, z), released with the test
    template<typename Height>
    engine_terrain_t create_terrain(const char* name, std::uint32_t samples_x, std::uint32_t samples_z, float cell_size, Height height)
    {
        std::vector<float> heights;
        for (std::uint32_t z = 0; z < samples_z; z++)
        {
            for (std::uint32_t x = 0; x < samples_x; x++)
            {
                heights.push_back(height(static_cast<float>(x), static_cast<float>(z)));
            }
        }
        const engine_terrain_create_desc_t desc{ samples_x, samples_z, cell_size, heights.data() };
        engine_terrain_t terrain = ENGINE_INVALID_OBJECT_HANDLE;
        EXPECT_EQ(engineTerrainCreate(name, &desc, &terrain), ENGINE_RESULT_CODE_OK);
        terrains_.push_back(terrain);
        return terrain;
    }

    // a game object at `position` drawing the terrain and colliding as it
    engine_game_object_t place_terrain(engine_terrain_t terrain, const glm::vec3& position = glm::vec3(0.0f))
    {
        const auto go = engineGameObjectCreate();
        auto tc = engineGameObjectAddTransformComponent(go);
        tc.position[0] = position.x;
        tc.position[1] = position.y;
        tc.position[2] = position.z;
        engineGameObjectUpdateTransformComponent(go, &tc);
        auto terrain_component = engineGameObjectAddTerrainComponent(go);
        terrain_component.terrain = terrain;
        engineGameObjectUpdateTerrainComponent(go, &terrain_component);
        engineGameObjectAddMaterialComponent(go);
        auto collider = engineGameObjectAddColliderComponent(go);
        collider.type = ENGINE_COLLIDER_TYPE_HEIGHTFIELD;
        collider.collider.heightfield.terrain = terrain;
        engineGameObjectUpdateColliderComponent(go, &collider);
        return go;
    }

    // the terrain's height under a world position, found by a ray from high above
    static bool ray_down(float x, float z, engine_ray_hit_t& hit)
    {
        const float origin[3] = { x, 100.0f, z };
        const float down[3] = { 0.0f, -1.0f, 0.0f };
        return enginePhysicsRayCast(origin, down, 200.0f, nullptr, &hit);
    }

    static float sample(engine_game_object_t terrain, float x, float z)
    {
        const float position[3] = { x, 0.0f, z };
        float height = -1000.0f;
        EXPECT_EQ(engineTerrainSampleHeight(terrain, position, &height, nullptr), ENGINE_RESULT_CODE_OK);
        return height;
    }

    // a file in the temporary folder, removed with the test
    std::string temp_file(const char* name)
    {
        const auto path = std::filesystem::temp_directory_path() / name;
        files_.push_back(path);
        return path.generic_string();
    }

    std::vector<engine_terrain_t> terrains_;
    std::vector<std::filesystem::path> files_;
};

// rises half a metre per metre along x
float slope(float x, float /*z*/)
{
    return x;
}
}  // namespace

TEST_F(TerrainTest, CreatedTerrainReportsItsSize)
{
    const auto terrain = create_terrain("test/info", 17, 9, 2.0f, slope);
    engine_terrain_info_t info{};
    ASSERT_EQ(engineTerrainGetInfo(terrain, &info), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(info.samples_x, 17u);
    EXPECT_EQ(info.samples_z, 9u);
    EXPECT_FLOAT_EQ(info.cell_size, 2.0f);
    EXPECT_FLOAT_EQ(info.size_x, 32.0f);
    EXPECT_FLOAT_EQ(info.size_z, 16.0f);
    EXPECT_FLOAT_EQ(info.height_min, 0.0f);
    EXPECT_FLOAT_EQ(info.height_max, 16.0f);
    EXPECT_EQ(engineTerrainGetByName("test/info"), terrain);

    // a name is used once
    const engine_terrain_create_desc_t desc{ 2, 2, 1.0f, nullptr };
    engine_terrain_t second = ENGINE_INVALID_OBJECT_HANDLE;
    EXPECT_EQ(engineTerrainCreate("test/info", &desc, &second), ENGINE_RESULT_CODE_FAIL);
    // and a terrain has a size
    const engine_terrain_create_desc_t no_size{ 1, 2, 1.0f, nullptr };
    EXPECT_EQ(engineTerrainCreate("test/no_size", &no_size, &second), ENGINE_RESULT_CODE_INVALID_ARG);
}

TEST_F(TerrainTest, RayCastHitsTheSurface)
{
    const auto terrain = create_terrain("test/slope", 17, 17, 2.0f, slope);
    const auto go = place_terrain(terrain, { 10.0f, 1.0f, -5.0f });

    // 7.3 m into the terrain along x: 3.65 m up its slope
    engine_ray_hit_t hit{};
    ASSERT_TRUE(ray_down(17.3f, 4.1f, hit));
    EXPECT_EQ(hit.go, go);
    EXPECT_NEAR(hit.position[1], 1.0f + 3.65f, 0.01f);
    // the slope's normal leans back along x
    EXPECT_NEAR(hit.normal[0], -0.4472f, 0.01f);
    EXPECT_NEAR(hit.normal[1], 0.8944f, 0.01f);
    EXPECT_NEAR(hit.normal[2], 0.0f, 0.01f);

    // beside the terrain there is nothing
    EXPECT_FALSE(ray_down(9.5f, 0.0f, hit));
    EXPECT_FALSE(ray_down(42.5f, 0.0f, hit));
    EXPECT_FALSE(ray_down(20.0f, 27.5f, hit));
}

TEST_F(TerrainTest, SampleHeightIsTheSurfaceTheRayHits)
{
    // not a plane: the two triangles of a cell differ
    const auto terrain = create_terrain("test/bumps", 9, 9, 1.5f, [](float x, float z) { return std::sin(x * 1.3f) + std::cos(z * 0.9f) * 2.0f; });
    const auto go = place_terrain(terrain, { -3.0f, 2.0f, 4.0f });

    for (const auto& point : { glm::vec2(-2.1f, 4.3f), glm::vec2(0.4f, 9.9f), glm::vec2(5.25f, 6.6f), glm::vec2(8.9f, 15.9f) })
    {
        engine_ray_hit_t hit{};
        ASSERT_TRUE(ray_down(point.x, point.y, hit)) << point.x << ", " << point.y;
        EXPECT_NEAR(sample(go, point.x, point.y), hit.position[1], 0.01f) << point.x << ", " << point.y;
    }

    const float outside[3] = { -3.5f, 0.0f, 5.0f };
    float height = 0.0f;
    EXPECT_EQ(engineTerrainSampleHeight(go, outside, &height, nullptr), ENGINE_RESULT_CODE_FAIL);
    // a game object without a terrain
    const float inside[3] = { 0.0f, 0.0f, 6.0f };
    EXPECT_EQ(engineTerrainSampleHeight(engineGameObjectCreate(), inside, &height, nullptr), ENGINE_RESULT_CODE_FAIL);
}

TEST_F(TerrainTest, TurnedAndScaledTerrainCollidesWhereItsTransformPutsIt)
{
    const auto terrain = create_terrain("test/turned", 9, 5, 1.0f, slope);
    const auto go = place_terrain(terrain, { 2.0f, 0.0f, 3.0f });
    auto tc = engineGameObjectGetTransformComponent(go);
    // a quarter turn around y: the terrain's x axis points along world -z; twice as long, three times as high
    const auto rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    tc.rotation[0] = rotation.x;
    tc.rotation[1] = rotation.y;
    tc.rotation[2] = rotation.z;
    tc.rotation[3] = rotation.w;
    tc.scale[0] = 2.0f;
    tc.scale[1] = 3.0f;
    tc.scale[2] = 2.0f;
    engineGameObjectUpdateTransformComponent(go, &tc);

    // 6 m along world -z is local x = 3: height 3, times 3
    engine_ray_hit_t hit{};
    ASSERT_TRUE(ray_down(5.0f, -3.0f, hit));
    EXPECT_NEAR(hit.position[1], 9.0f, 0.02f);
    EXPECT_NEAR(sample(go, 5.0f, -3.0f), 9.0f, 0.02f);
    // where it was before the turn there is nothing
    EXPECT_FALSE(ray_down(8.0f, 9.0f, hit));
}

TEST_F(TerrainTest, DynamicBodyRestsOnTheTerrain)
{
    const auto terrain = create_terrain("test/flat", 9, 9, 1.0f, [](float, float) { return 3.0f; });
    place_terrain(terrain, { -4.0f, 0.0f, -4.0f });

    const auto ball = engineGameObjectCreate();
    auto tc = engineGameObjectAddTransformComponent(ball);
    tc.position[1] = 5.0f;
    engineGameObjectUpdateTransformComponent(ball, &tc);
    auto rigid_body = engineGameObjectAddRigidBodyComponent(ball);
    rigid_body.mass = 1.0f;
    engineGameObjectUpdateRigidBodyComponent(ball, &rigid_body);
    auto collider = engineGameObjectAddColliderComponent(ball);
    collider.type = ENGINE_COLLIDER_TYPE_SPHERE;
    collider.collider.sphere.radius = 0.5f;
    collider.bounciness = 0.0f;
    engineGameObjectUpdateColliderComponent(ball, &collider);

    step(180);

    EXPECT_NEAR(position_of(ball).y, 3.5f, 0.05f);
}

TEST_F(TerrainTest, CharacterWalksUpAndDownTheTerrain)
{
    // a hill: up at 1 in 4 for 8 m, then down again
    const auto terrain = create_terrain("test/hill", 17, 9, 1.0f, [](float x, float) { return (x <= 8.0f ? x : 16.0f - x) * 0.25f; });
    const auto ground = place_terrain(terrain, { 0.0f, 0.0f, -4.0f });

    const auto character = engineGameObjectCreate();
    auto tc = engineGameObjectAddTransformComponent(character);
    tc.position[0] = 1.0f;
    tc.position[1] = 1.0f;
    engineGameObjectUpdateTransformComponent(character, &tc);
    auto controller = engineGameObjectAddCharacterControllerComponent(character);
    controller.radius = 0.3f;
    controller.height = 1.8f;
    engineGameObjectUpdateCharacterControllerComponent(character, &controller);

    engine_character_controller_move_result_t result{};
    const auto walk = [&](const glm::vec3& velocity, std::uint32_t frames)
        {
            for (std::uint32_t i = 0; i < frames; i++)
            {
                EXPECT_EQ(engineCharacterControllerMove(character, &velocity.x, scenario::FIXED_DT_MS, &result), ENGINE_RESULT_CODE_OK);
                step();
            }
        };
    walk(glm::vec3(0.0f), 60);
    ASSERT_TRUE(result.grounded);
    EXPECT_NEAR(position_of(character).y, sample(ground, 1.0f, 0.0f), 0.05f);

    // up the hill
    walk({ 3.0f, 0.0f, 0.0f }, 100);
    auto position = position_of(character);
    EXPECT_NEAR(position.x, 6.0f, 0.3f);
    EXPECT_TRUE(result.grounded);
    EXPECT_NEAR(position.y, sample(ground, position.x, position.z), 0.06f);
    EXPECT_GT(position.y, 1.0f);

    // over the top and down the other side, staying on the ground
    walk({ 3.0f, 0.0f, 0.0f }, 160);
    position = position_of(character);
    EXPECT_NEAR(position.x, 14.0f, 0.4f);
    EXPECT_TRUE(result.grounded);
    EXPECT_NEAR(position.y, sample(ground, position.x, position.z), 0.06f);
    EXPECT_LT(position.y, 1.0f);
}

TEST_F(TerrainTest, ChangedHeightsAreCollidedWithAtOnce)
{
    const auto terrain = create_terrain("test/changing", 9, 9, 1.0f, [](float, float) { return 0.0f; });
    const auto go = place_terrain(terrain);
    engine_ray_hit_t hit{};
    ASSERT_TRUE(ray_down(4.0f, 4.0f, hit));
    EXPECT_NEAR(hit.position[1], 0.0f, 0.01f);

    // a plateau of 3 x 3 samples, above the old height range
    const std::vector<float> plateau(9, 5.0f);
    ASSERT_EQ(engineTerrainSetHeights(terrain, 3, 3, 3, 3, plateau.data()), ENGINE_RESULT_CODE_OK);

    // no frame in between
    ASSERT_TRUE(ray_down(4.0f, 4.0f, hit));
    EXPECT_NEAR(hit.position[1], 5.0f, 0.01f);
    EXPECT_NEAR(sample(go, 4.0f, 4.0f), 5.0f, 1e-4f);
    // its side slopes down to the old ground
    ASSERT_TRUE(ray_down(2.5f, 4.0f, hit));
    EXPECT_NEAR(hit.position[1], 2.5f, 0.01f);
    ASSERT_TRUE(ray_down(1.0f, 1.0f, hit));
    EXPECT_NEAR(hit.position[1], 0.0f, 0.01f);

    std::vector<float> read(9);
    ASSERT_EQ(engineTerrainGetHeights(terrain, 3, 3, 3, 3, read.data()), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(read, plateau);
    engine_terrain_info_t info{};
    ASSERT_EQ(engineTerrainGetInfo(terrain, &info), ENGINE_RESULT_CODE_OK);
    EXPECT_FLOAT_EQ(info.height_max, 5.0f);

    // a region reaching out of the terrain changes nothing
    EXPECT_EQ(engineTerrainSetHeights(terrain, 7, 7, 3, 3, plateau.data()), ENGINE_RESULT_CODE_INVALID_ARG);
    EXPECT_EQ(engineTerrainGetHeights(terrain, 7, 7, 3, 3, read.data()), ENGINE_RESULT_CODE_INVALID_ARG);
    step(2);
    ASSERT_TRUE(ray_down(7.5f, 7.5f, hit));
    EXPECT_NEAR(hit.position[1], 0.0f, 0.01f);
}

TEST_F(TerrainTest, ColliderWithoutItsTerrainHasNoBodyUntilItGetsOne)
{
    const auto go = engineGameObjectCreate();
    engineGameObjectAddTransformComponent(go);
    auto collider = engineGameObjectAddColliderComponent(go);
    collider.type = ENGINE_COLLIDER_TYPE_HEIGHTFIELD;
    engineGameObjectUpdateColliderComponent(go, &collider);
    engine_ray_hit_t hit{};
    EXPECT_FALSE(ray_down(2.0f, 2.0f, hit));

    collider.collider.heightfield.terrain = create_terrain("test/late", 5, 5, 1.0f, [](float, float) { return 1.0f; });
    engineGameObjectUpdateColliderComponent(go, &collider);
    ASSERT_TRUE(ray_down(2.0f, 2.0f, hit));
    EXPECT_NEAR(hit.position[1], 1.0f, 0.01f);

    // disabled like any collider
    collider.disable = true;
    engineGameObjectUpdateColliderComponent(go, &collider);
    EXPECT_FALSE(ray_down(2.0f, 2.0f, hit));
    // heights changed while it is disabled give it no body, and are there when it is enabled again
    const std::vector<float> raised(25, 4.0f);
    ASSERT_EQ(engineTerrainSetHeights(collider.collider.heightfield.terrain, 0, 0, 5, 5, raised.data()), ENGINE_RESULT_CODE_OK);
    EXPECT_FALSE(ray_down(2.0f, 2.0f, hit));
    step(2);
    EXPECT_FALSE(ray_down(2.0f, 2.0f, hit));
    collider.disable = false;
    engineGameObjectUpdateColliderComponent(go, &collider);
    ASSERT_TRUE(ray_down(2.0f, 2.0f, hit));
    EXPECT_NEAR(hit.position[1], 4.0f, 0.01f);
}

// the committed terrain of the outdoor level is what importing its heightmap writes: pressing Import in the editor
// with the same settings changes nothing (generate_test_terrains.py computes the heights like the engine)
TEST_F(TerrainTest, OutdoorTerrainIsItsHeightmapImported)
{
    const engine_terrain_import_desc_t desc{ 1.0f, 0.0f, 6.0f };
    engine_terrain_t imported = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainImportHeightmap("outdoor_test.png", &desc, "test/outdoor_imported", &imported), ENGINE_RESULT_CODE_OK);
    terrains_.push_back(imported);
    engine_terrain_t committed = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainLoad("outdoor_test.terrain", &committed), ENGINE_RESULT_CODE_OK);
    terrains_.push_back(committed);
    engine_terrain_info_t info{};
    ASSERT_EQ(engineTerrainGetInfo(committed, &info), ENGINE_RESULT_CODE_OK);
    ASSERT_EQ(info.samples_x, 65u);
    ASSERT_EQ(info.samples_z, 65u);
    EXPECT_FLOAT_EQ(info.cell_size, 1.0f);
    std::vector<float> from_file(65 * 65);
    std::vector<float> from_heightmap(65 * 65);
    ASSERT_EQ(engineTerrainGetHeights(committed, 0, 0, 65, 65, from_file.data()), ENGINE_RESULT_CODE_OK);
    ASSERT_EQ(engineTerrainGetHeights(imported, 0, 0, 65, 65, from_heightmap.data()), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(from_file, from_heightmap);
}

TEST_F(TerrainTest, HeightfieldStaysStaticWhateverRigidBodyItHas)
{
    const auto terrain = create_terrain("test/static", 5, 5, 1.0f, [](float, float) { return 1.0f; });
    const auto go = place_terrain(terrain);
    auto rigid_body = engineGameObjectAddRigidBodyComponent(go);
    rigid_body.type = ENGINE_RIGID_BODY_TYPE_DYNAMIC;
    rigid_body.mass = 10.0f;
    engineGameObjectUpdateRigidBodyComponent(go, &rigid_body);

    step(60);

    // it didn't fall
    EXPECT_NEAR(position_of(go).y, 0.0f, 1e-5f);
    engine_ray_hit_t hit{};
    ASSERT_TRUE(ray_down(2.0f, 2.0f, hit));
    EXPECT_NEAR(hit.position[1], 1.0f, 0.01f);
}

TEST_F(TerrainTest, NavMeshFollowsTheTerrain)
{
    // a slope agents can walk (1 in 4), with a wall too steep for them across half of it
    const auto terrain = create_terrain("test/nav", 17, 17, 1.0f, [](float x, float z)
        {
            const bool wall = x >= 8.0f && x <= 9.0f && z <= 10.0f;
            return x * 0.25f + (wall ? 4.0f : 0.0f);
        });
    place_terrain(terrain);
    engineGameObjectAddNavSurfaceComponent(engineGameObjectCreate());
    ASSERT_EQ(engineNavMeshBuild(), ENGINE_RESULT_CODE_OK);

    const float start[3] = { 3.0f, 0.75f, 3.0f };
    const float end[3] = { 13.0f, 3.25f, 3.0f };
    const float extents[3] = { 1.0f, 1.0f, 1.0f };
    std::vector<float> points(64 * 3);
    size_t count = 0;
    engine_nav_path_status_t status{};
    ASSERT_EQ(engineNavFindPath(start, end, extents, points.data(), 64, &count, &status), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(status, ENGINE_NAV_PATH_STATUS_COMPLETE);
    // around the wall: the path goes past z = 10, and it is longer than the straight line
    ASSERT_GE(count, 3u);
    float furthest_z = 0.0f;
    float length = 0.0f;
    for (size_t i = 0; i < count; i++)
    {
        furthest_z = std::max(furthest_z, points[i * 3 + 2]);
        if (i > 0)
        {
            length += glm::distance(glm::vec3(points[i * 3], points[i * 3 + 1], points[i * 3 + 2]), glm::vec3(points[i * 3 - 3], points[i * 3 - 2], points[i * 3 - 1]));
        }
    }
    EXPECT_GT(furthest_z, 10.0f);
    EXPECT_GT(length, 15.0f);
    // the nav mesh lies on the slope
    float nearest[3] = {};
    const float above[3] = { 4.0f, 2.0f, 13.0f };
    const float far_extents[3] = { 0.5f, 3.0f, 0.5f };
    ASSERT_EQ(engineNavFindNearestPoint(above, far_extents, nearest), ENGINE_RESULT_CODE_OK);
    EXPECT_NEAR(nearest[1], 1.0f, 0.15f);
}

TEST_F(TerrainTest, SavedTerrainLoadsWithTheSameHeights)
{
    const auto name = temp_file("project_c_terrain_test.terrain");
    const auto terrain = create_terrain(name.c_str(), 33, 17, 0.5f, [](float x, float z) { return std::sin(x) * 2.0f + z * 0.1f; });
    ASSERT_EQ(engineTerrainSave(terrain), ENGINE_RESULT_CODE_OK);
    std::vector<float> saved(33 * 17);
    ASSERT_EQ(engineTerrainGetHeights(terrain, 0, 0, 33, 17, saved.data()), ENGINE_RESULT_CODE_OK);

    // loading a loaded terrain gives the same one
    engine_terrain_t again = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainLoad(name.c_str(), &again), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(again, terrain);
    engineTerrainRelease(again);

    // released and read from its file
    engineTerrainRelease(terrain);
    terrains_.clear();
    EXPECT_EQ(engineTerrainGetByName(name.c_str()), ENGINE_INVALID_OBJECT_HANDLE);
    engine_terrain_t loaded = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainLoad(name.c_str(), &loaded), ENGINE_RESULT_CODE_OK);
    terrains_.push_back(loaded);
    engine_terrain_info_t info{};
    ASSERT_EQ(engineTerrainGetInfo(loaded, &info), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(info.samples_x, 33u);
    EXPECT_EQ(info.samples_z, 17u);
    EXPECT_FLOAT_EQ(info.cell_size, 0.5f);
    std::vector<float> read(33 * 17);
    ASSERT_EQ(engineTerrainGetHeights(loaded, 0, 0, 33, 17, read.data()), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(read, saved);
}

TEST_F(TerrainTest, TerrainWhichIsNoFileCanNotBeSavedOrLoaded)
{
    const auto terrain = create_terrain("test/not_a_file", 3, 3, 1.0f, slope);
    EXPECT_EQ(engineTerrainSave(terrain), ENGINE_RESULT_CODE_FAIL);
    engine_terrain_t loaded = ENGINE_INVALID_OBJECT_HANDLE;
    EXPECT_EQ(engineTerrainLoad("test/no_such_file.terrain", &loaded), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(loaded, ENGINE_INVALID_OBJECT_HANDLE);
    // a file which is something else
    const auto other = temp_file("project_c_terrain_test_other.terrain");
    std::ofstream(other) << "{ \"version\": 1, \"entities\": [] }";
    EXPECT_EQ(engineTerrainLoad(other.c_str(), &loaded), ENGINE_RESULT_CODE_FAIL);
}

TEST_F(TerrainTest, ImportsA16BitPngHeightmap)
{
    // pixel (x, y) of the fixture is 1000 * x + 10 * y (generate_test_terrains.py)
    const engine_terrain_import_desc_t desc{ 2.0f, 0.0f, 65.535f };
    engine_terrain_t terrain = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainImportHeightmap("test_fixtures/hills.png", &desc, "test/imported", &terrain), ENGINE_RESULT_CODE_OK);
    terrains_.push_back(terrain);
    engine_terrain_info_t info{};
    ASSERT_EQ(engineTerrainGetInfo(terrain, &info), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(info.samples_x, 65u);
    EXPECT_EQ(info.samples_z, 65u);
    EXPECT_FLOAT_EQ(info.size_x, 128.0f);
    // with this range a pixel value is a height in millimetres
    float heights[2] = {};
    ASSERT_EQ(engineTerrainGetHeights(terrain, 3, 7, 2, 1, heights), ENGINE_RESULT_CODE_OK);
    EXPECT_NEAR(heights[0], 3.07f, 1e-4f);
    EXPECT_NEAR(heights[1], 4.07f, 1e-4f);
    EXPECT_NEAR(info.height_max, 64.64f, 1e-3f);

    // imported again under the same name: the same terrain, with the new heights
    const engine_terrain_import_desc_t taller{ 2.0f, 100.0f, 165.535f };
    engine_terrain_t again = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainImportHeightmap("test_fixtures/hills.png", &taller, "test/imported", &again), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(again, terrain);
    terrains_.push_back(again);
    ASSERT_EQ(engineTerrainGetHeights(terrain, 3, 7, 2, 1, heights), ENGINE_RESULT_CODE_OK);
    EXPECT_NEAR(heights[0], 103.07f, 1e-3f);
}

TEST_F(TerrainTest, ReimportedTerrainIsCollidedWithAtItsNewHeights)
{
    const engine_terrain_import_desc_t desc{ 1.0f, 0.0f, 65.535f };
    engine_terrain_t terrain = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainImportHeightmap("test_fixtures/hills.png", &desc, "test/reimported", &terrain), ENGINE_RESULT_CODE_OK);
    terrains_.push_back(terrain);
    place_terrain(terrain);
    engine_ray_hit_t hit{};
    ASSERT_TRUE(ray_down(10.0f, 20.0f, hit));
    EXPECT_NEAR(hit.position[1], 10.2f, 0.01f);

    const engine_terrain_import_desc_t lower{ 1.0f, -50.0f, 15.535f };
    engine_terrain_t again = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainImportHeightmap("test_fixtures/hills.png", &lower, "test/reimported", &again), ENGINE_RESULT_CODE_OK);
    terrains_.push_back(again);

    ASSERT_TRUE(ray_down(10.0f, 20.0f, hit));
    EXPECT_NEAR(hit.position[1], -39.8f, 0.01f);
}

TEST_F(TerrainTest, ImportsARawHeightmapAndRefusesWhatIsNone)
{
    // 3 x 3 little endian pixels: 0, 256, 512, ...
    const auto raw = temp_file("project_c_terrain_test.r16");
    {
        std::ofstream file(raw, std::ios::binary);
        for (std::uint16_t i = 0; i < 9; i++)
        {
            const char bytes[2] = { 0, static_cast<char>(i) };
            file.write(bytes, 2);
        }
    }
    const engine_terrain_import_desc_t desc{ 1.0f, 0.0f, 65535.0f };
    engine_terrain_t terrain = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainImportHeightmap(raw.c_str(), &desc, "test/raw", &terrain), ENGINE_RESULT_CODE_OK);
    terrains_.push_back(terrain);
    float heights[9] = {};
    ASSERT_EQ(engineTerrainGetHeights(terrain, 0, 0, 3, 3, heights), ENGINE_RESULT_CODE_OK);
    EXPECT_FLOAT_EQ(heights[1], 256.0f);
    EXPECT_FLOAT_EQ(heights[8], 2048.0f);

    engine_terrain_t none = ENGINE_INVALID_OBJECT_HANDLE;
    EXPECT_EQ(engineTerrainImportHeightmap("test_fixtures/no_such.png", &desc, "test/none", &none), ENGINE_RESULT_CODE_FAIL);
    // an 8-bit image (a texture of the game) isn't a heightmap
    const auto eight_bit = std::string(TEST_ASSETS_DIR) + "/textures/container.png";
    if (std::filesystem::exists(eight_bit))
    {
        EXPECT_EQ(engineTerrainImportHeightmap(eight_bit.c_str(), &desc, "test/none", &none), ENGINE_RESULT_CODE_FAIL);
    }
    // a range which isn't one
    const engine_terrain_import_desc_t upside_down{ 1.0f, 10.0f, 0.0f };
    EXPECT_EQ(engineTerrainImportHeightmap(raw.c_str(), &upside_down, "test/none", &none), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(engineTerrainGetByName("test/none"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(TerrainTest, SceneFileKeepsTheTerrainByItsAssetId)
{
    const auto name = temp_file("project_c_terrain_scene_test.terrain");
    const auto scene_file = temp_file("project_c_terrain_scene_test.json");
    {
        const auto terrain = create_terrain(name.c_str(), 9, 9, 1.0f, slope);
        ASSERT_EQ(engineTerrainSave(terrain), ENGINE_RESULT_CODE_OK);
        const auto go = place_terrain(terrain, { 1.0f, 2.0f, 3.0f });
        auto terrain_component = engineGameObjectGetTerrainComponent(go);
        terrain_component.texture_tile_size = 2.5f;
        engineGameObjectUpdateTerrainComponent(go, &terrain_component);
        ASSERT_EQ(engineSceneSave(scene_file.c_str()), ENGINE_RESULT_CODE_OK);
        // nothing of it stays: the next scene reads everything from the files
        engineTerrainRelease(terrain);
        terrains_.clear();
    }

    new_scene();
    ASSERT_EQ(engineTerrainGetByName(name.c_str()), ENGINE_INVALID_OBJECT_HANDLE);
    ASSERT_EQ(engineSceneLoad(scene_file.c_str()), ENGINE_RESULT_CODE_OK);

    // the scene holds the terrain it loaded
    const auto terrain = engineTerrainGetByName(name.c_str());
    ASSERT_NE(terrain, ENGINE_INVALID_OBJECT_HANDLE);
    engine_ray_hit_t hit{};
    ASSERT_TRUE(ray_down(5.0f, 6.0f, hit));
    EXPECT_NEAR(hit.position[1], 2.0f + 4.0f, 0.01f);
    const auto go = hit.go;
    ASSERT_TRUE(engineGameObjectHasTerrainComponent(go));
    const auto terrain_component = engineGameObjectGetTerrainComponent(go);
    EXPECT_EQ(terrain_component.terrain, terrain);
    EXPECT_FLOAT_EQ(terrain_component.texture_tile_size, 2.5f);
    const auto collider = engineGameObjectGetColliderComponent(go);
    EXPECT_EQ(collider.type, ENGINE_COLLIDER_TYPE_HEIGHTFIELD);
    EXPECT_EQ(collider.collider.heightfield.terrain, terrain);
    EXPECT_NEAR(sample(go, 5.0f, 6.0f), 6.0f, 1e-4f);
    step(3);
}

TEST_F(TerrainTest, TerrainMadeByCodeIsFoundByASceneFileWhileItExists)
{
    const auto scene_file = temp_file("project_c_terrain_generated_test.json");
    const auto terrain = create_terrain("test/generated", 5, 5, 1.0f, [](float, float) { return 2.0f; });
    place_terrain(terrain);
    ASSERT_EQ(engineSceneSave(scene_file.c_str()), ENGINE_RESULT_CODE_OK);

    new_scene();
    ASSERT_EQ(engineSceneLoad(scene_file.c_str()), ENGINE_RESULT_CODE_OK);

    engine_ray_hit_t hit{};
    ASSERT_TRUE(ray_down(2.0f, 2.0f, hit));
    EXPECT_NEAR(hit.position[1], 2.0f, 0.01f);
    EXPECT_EQ(engineGameObjectGetTerrainComponent(hit.go).terrain, terrain);
}

TEST_F(TerrainTest, DrawnTerrainSurvivesItsAssetBeingReleased)
{
    // what the renderer and the physics do when the game releases a terrain a scene still shows: nothing crashes
    const engine_terrain_create_desc_t desc{ 65, 65, 1.0f, nullptr };
    engine_terrain_t terrain = ENGINE_INVALID_OBJECT_HANDLE;
    ASSERT_EQ(engineTerrainCreate("test/released", &desc, &terrain), ENGINE_RESULT_CODE_OK);
    place_terrain(terrain);
    const auto camera = engineGameObjectCreate();
    auto camera_transform = engineGameObjectAddTransformComponent(camera);
    camera_transform.position[0] = 32.0f;
    camera_transform.position[1] = 20.0f;
    camera_transform.position[2] = 60.0f;
    engineGameObjectUpdateTransformComponent(camera, &camera_transform);
    auto camera_component = engineGameObjectAddCameraComponent(camera);
    camera_component.enabled = true;
    camera_component.target[0] = 32.0f;
    camera_component.target[2] = 32.0f;
    engineGameObjectUpdateCameraComponent(camera, &camera_component);
    step(3);
    engine_ray_hit_t hit{};
    ASSERT_TRUE(ray_down(10.0f, 10.0f, hit));

    engineTerrainRelease(terrain);
    step(3);

    // the body is gone with the heights
    EXPECT_FALSE(ray_down(10.0f, 10.0f, hit));
}
