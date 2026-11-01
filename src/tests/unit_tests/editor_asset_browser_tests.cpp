#include "editor_asset_browser.h"

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
// an assets folder of its own in the temp folder, removed with the test
class AssetBrowserTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
        root_ = std::filesystem::temp_directory_path() / (std::string("engine_asset_browser_") + test->name());
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(root_);
    }

    void add_file(const std::string& path)
    {
        const auto file = root_ / path;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file) << "x";
    }

    void add_folder(const std::string& path)
    {
        std::filesystem::create_directories(root_ / path);
    }

    std::vector<std::string> names(const std::string& folder) const
    {
        std::vector<std::string> result;
        for (const auto& entry : engine::list_asset_folder(root_, folder))
        {
            result.push_back(entry.name);
        }
        return result;
    }

    std::filesystem::path root_;
};

engine::asset_entry_t entry(const std::string& path, bool is_folder = false)
{
    return { std::filesystem::path(path).filename().string(), path, engine::get_asset_file_kind(path, is_folder) };
}
}  // namespace

TEST(AssetFileKind, ToldByExtensionIgnoringCase)
{
    using engine::AssetFileKind;
    EXPECT_EQ(engine::get_asset_file_kind("models/a/Rogue.glb", false), AssetFileKind::eModel);
    EXPECT_EQ(engine::get_asset_file_kind("models/a/Rogue.GLTF", false), AssetFileKind::eModel);
    EXPECT_EQ(engine::get_asset_file_kind("models/a/rogue_texture.PNG", false), AssetFileKind::eTexture);
    EXPECT_EQ(engine::get_asset_file_kind("textures/noise.jpg", false), AssetFileKind::eTexture);
    EXPECT_EQ(engine::get_asset_file_kind("shaders/simple.vs", false), AssetFileKind::eOther);
    EXPECT_EQ(engine::get_asset_file_kind("models/a.glb", true), AssetFileKind::eFolder);
}

TEST(AssetFileKind, JsonIsASceneUnderScenesAndADataAssetUnderData)
{
    using engine::AssetFileKind;
    EXPECT_EQ(engine::get_asset_file_kind("scenes/test_level.json", false), AssetFileKind::eScene);
    EXPECT_EQ(engine::get_asset_file_kind("scenes/rooms/room_a.json", false), AssetFileKind::eScene);
    EXPECT_EQ(engine::get_asset_file_kind("data/enemies/skeleton.json", false), AssetFileKind::eData);
    EXPECT_EQ(engine::get_asset_file_kind("scenes_old/level.json", false), AssetFileKind::eOther);
    EXPECT_EQ(engine::get_asset_file_kind("ui_docs/layout.json", false), AssetFileKind::eOther);
}

TEST_F(AssetBrowserTest, ListsFoldersFirstThenFilesByNameIgnoringCase)
{
    add_file("models/b.glb");
    add_file("models/A.gltf");
    add_file("models/c.png");
    add_folder("models/zeta");
    add_folder("models/Alpha");
    EXPECT_EQ(names("models"), (std::vector<std::string>{ "Alpha", "zeta", "A.gltf", "b.glb", "c.png" }));
}

TEST_F(AssetBrowserTest, EntriesHaveTheirPathAndKind)
{
    add_file("scenes/rooms/room_a.json");
    const auto root = engine::list_asset_folder(root_, "");
    ASSERT_EQ(root.size(), 1u);
    EXPECT_EQ(root[0].path, "scenes");
    EXPECT_EQ(root[0].kind, engine::AssetFileKind::eFolder);
    const auto rooms = engine::list_asset_folder(root_, "scenes/rooms");
    ASSERT_EQ(rooms.size(), 1u);
    EXPECT_EQ(rooms[0].name, "room_a.json");
    EXPECT_EQ(rooms[0].path, "scenes/rooms/room_a.json");
    EXPECT_EQ(rooms[0].kind, engine::AssetFileKind::eScene);
}

TEST_F(AssetBrowserTest, LeavesOutGltfBuffers)
{
    add_file("models/house.gltf");
    add_file("models/house.bin");
    add_file("models/HOUSE2.BIN");
    EXPECT_EQ(names("models"), (std::vector<std::string>{ "house.gltf" }));
}

TEST_F(AssetBrowserTest, MissingFolderListsNothing)
{
    EXPECT_TRUE(engine::list_asset_folder(root_, "nothing/here").empty());
}

TEST(AssetBrowserAssetId, ModelsAndImagesByTheirPathUnderModels)
{
    EXPECT_EQ(engine::get_asset_id(entry("models/KayKit/Characters/gltf/Rogue.glb")), "KayKit/Characters/gltf/Rogue.glb");
    EXPECT_EQ(engine::get_asset_id(entry("models/KayKit/Textures/rogue_texture.png")), "KayKit/Textures/rogue_texture.png");
}

TEST(AssetBrowserAssetId, ScenesByTheirPathUnderScenes)
{
    EXPECT_EQ(engine::get_asset_id(entry("scenes/rooms/room_a.json")), "rooms/room_a.json");
}

TEST(AssetBrowserAssetId, DataAssetsByTheirPathUnderData)
{
    EXPECT_EQ(engine::get_asset_id(entry("data/enemies/skeleton_warrior.json")), "enemies/skeleton_warrior.json");
}

TEST(AssetBrowserAssetId, NoneOutsideTheirFolderOrForOtherFiles)
{
    // images outside models/ aren't named by a path the engine resolves
    EXPECT_FALSE(engine::get_asset_id(entry("textures/noise.png")).has_value());
    EXPECT_FALSE(engine::get_asset_id(entry("Rogue.glb")).has_value());
    EXPECT_FALSE(engine::get_asset_id(entry("shaders/simple.vs")).has_value());
    EXPECT_FALSE(engine::get_asset_id(entry("models/characters", true)).has_value());
    // "modelsX/" is not under "models/"
    EXPECT_FALSE(engine::get_asset_id(entry("modelsX/a.glb")).has_value());
}

TEST(AssetBrowserModelAssets, OnlyTheModelsOwnInTheirOrder)
{
    const std::vector<std::pair<std::uint32_t, std::string>> geometries = {
        { 1, "a/Rogue.glb#Body" }, { 2, "a/Rogue.glb.old#Body" }, { 3, "a/Knight.glb#Body" }, { 4, "a/Rogue.glb#mesh_1" }, { 5, "a/Rogue.glb" } };
    const auto model_assets = engine::get_model_assets(geometries, "a/Rogue.glb");
    ASSERT_EQ(model_assets.size(), 2u);
    EXPECT_EQ(model_assets[0].first, 1u);
    EXPECT_EQ(model_assets[1].second, "a/Rogue.glb#mesh_1");
}

TEST(AssetBrowserGroundPoint, WhereTheRayMeetsTheGround)
{
    const auto point = engine::get_ground_point(glm::vec3(1.0f, 10.0f, 2.0f), glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f)), 100.0f);
    EXPECT_NEAR(point.x, 11.0f, 1e-4f);
    EXPECT_NEAR(point.y, 0.0f, 1e-4f);
    EXPECT_NEAR(point.z, 2.0f, 1e-4f);
}

TEST(AssetBrowserGroundPoint, InFrontOfTheCameraWhenTheRayMissesTheGround)
{
    // looking up, and looking down from below the ground
    const auto up = engine::get_ground_point(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 100.0f, 5.0f);
    EXPECT_NEAR(up.y, 6.0f, 1e-4f);
    const auto below = engine::get_ground_point(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 100.0f, 5.0f);
    EXPECT_NEAR(below.y, -6.0f, 1e-4f);
}

TEST(AssetBrowserGroundPoint, InFrontOfTheCameraWhenTheGroundIsBeyondMaxDistance)
{
    // just below the horizon: the ground is 50000 units away
    const auto direction = glm::normalize(glm::vec3(1.0f, -0.0002f, 0.0f));
    const auto point = engine::get_ground_point(glm::vec3(0.0f, 10.0f, 0.0f), direction, 100.0f, 5.0f);
    EXPECT_NEAR(glm::length(point - glm::vec3(0.0f, 10.0f, 0.0f)), 5.0f, 1e-4f);
}

TEST(AssetBrowserSceneSavePath, TheNameInItsFolderUnderScenes)
{
    EXPECT_EQ(engine::get_scene_save_path("scenes", "level").value_or(""), "level.json");
    EXPECT_EQ(engine::get_scene_save_path("scenes/rooms", "room_a.json").value_or(""), "rooms/room_a.json");
    EXPECT_EQ(engine::get_scene_save_path("scenes/rooms/big", "Room A.JSON").value_or(""), "rooms/big/Room A.JSON");
}

TEST(AssetBrowserSceneSavePath, NothingOutsideTheScenesFolder)
{
    EXPECT_FALSE(engine::get_scene_save_path("data", "level").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("", "level").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenesX", "level").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes/../data", "level").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes/./rooms", "level").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes/rooms/..", "level").has_value());
}

TEST(AssetBrowserSceneSavePath, AFolderNameWithDotsIsOne)
{
    EXPECT_EQ(engine::get_scene_save_path("scenes/level..old", "level").value_or(""), "level..old/level.json");
}

TEST(AssetBrowserSceneSavePath, NothingForANameWhichIsntOneFileName)
{
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "..").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "rooms/a").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "rooms\a").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "a:b").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "level.").has_value());
    // Windows' device names
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "nul").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "CON.json").has_value());
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "com1").has_value());
    EXPECT_TRUE(engine::get_scene_save_path("scenes", "console").has_value());
    EXPECT_TRUE(engine::get_scene_save_path("scenes", "com0").has_value());
    // a scene file is JSON
    EXPECT_FALSE(engine::get_scene_save_path("scenes", "level.txt").has_value());
}
