#include "scenario_harness.h"
#include "test_models.h"

#include <iscene.h>

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

// Scene files in a real scene: the assets they reference are loaded on demand and held by the scene, and loaded
// components reach the scene's systems (physics) like ones added through the C API.
namespace
{
using scenario::TestModel;

// a second scene, created and destroyed by the test itself to see what the scene holds
class FileScene : public engine::IScene
{
public:
    FileScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "scenario_scene_file"; }
};

class SceneFileTest : public scenario::ModelTest
{
protected:
    void TearDown() override
    {
        if (scenario::app().get_scene(FileScene::get_name()))
        {
            destroy_file_scene();
        }
        std::filesystem::remove(file_);
        ModelTest::TearDown();
    }

    // an absolute path: written outside the source tree's assets
    std::string write_file(const std::string& content)
    {
        std::ofstream(file_, std::ios::binary) << content;
        return file_.string();
    }

    void create_file_scene()
    {
        scenario::app().register_scene<FileScene>()->activate();
    }

    void destroy_file_scene()
    {
        scenario::app().unregister_scene(FileScene::get_name());
        scenario::app().get_scene(scenario::EmptyScene::get_name())->activate();
    }

    static std::string scene_with_mesh(const std::string& geometry)
    {
        return R"({ "version": 1, "entities": [ { "id": 1, "components": {
            "transform": { "position": [0, 0, 0] }, "mesh": { "geometry": ")" + geometry + R"(" } } } ] })";
    }

    // the first game object of the active scene with the component (has is engineGameObjectHas*Component)
    static engine_game_object_t find_with(bool (*has)(engine_game_object_t))
    {
        for (engine_game_object_t go = 1; go < 64; go++)
        {
            if (engineGameObjectIsValid(go) && has(go))
            {
                return go;
            }
        }
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }

    std::filesystem::path file_ = std::filesystem::temp_directory_path() / "project_c_scene_file_test.json";
};

TEST_F(SceneFileTest, ModelNobodyLoadedIsLoadedAndHeldByTheScene)
{
    ASSERT_EQ(enginePrefabGetByName("test_fixtures/crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
    create_file_scene();
    ASSERT_EQ(engineSceneLoad(write_file(scene_with_mesh("test_fixtures/crate.gltf#Plate")).c_str()), ENGINE_RESULT_CODE_OK);

    const auto go = find_with(engineGameObjectHasMeshComponent);
    ASSERT_NE(go, ENGINE_INVALID_GAME_OBJECT_ID);
    const auto geometry = engineGameObjectGetMeshComponent(go).geometry;
    EXPECT_NE(geometry, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(geometry, engineGeometryGetByName("test_fixtures/crate.gltf#Plate"));
    EXPECT_NE(enginePrefabGetByName("test_fixtures/crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);

    // released with the scene
    destroy_file_scene();
    EXPECT_EQ(enginePrefabGetByName("test_fixtures/crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineGeometryGetByName("test_fixtures/crate.gltf#Plate"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(SceneFileTest, SceneKeepsItsModelWhenTheGameReleasesIt)
{
    auto crate = std::make_unique<TestModel>("crate.gltf");
    ASSERT_TRUE(crate->loaded());
    create_file_scene();
    ASSERT_EQ(engineSceneLoad(write_file(scene_with_mesh(crate->asset("Plate"))).c_str()), ENGINE_RESULT_CODE_OK);

    crate.reset();
    EXPECT_NE(engineGeometryGetByName("test_fixtures/crate.gltf#Plate"), ENGINE_INVALID_OBJECT_HANDLE);
    destroy_file_scene();
    EXPECT_EQ(enginePrefabGetByName("test_fixtures/crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(SceneFileTest, ImageFileIsLoadedAndHeldByTheScene)
{
    create_file_scene();
    ASSERT_EQ(engineSceneLoad(write_file(R"({ "version": 1, "entities": [ { "id": 1, "components": {
        "material": { "type": "pong", "pong": { "diffuse_texture": "test_fixtures/test_texture.png" } } } } ] })").c_str()), ENGINE_RESULT_CODE_OK);
    const auto go = find_with(engineGameObjectHasMaterialComponent);
    ASSERT_NE(go, ENGINE_INVALID_GAME_OBJECT_ID);
    const auto texture = engineGameObjectGetMaterialComponent(go).data.pong.diffuse_texture;
    EXPECT_NE(texture, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(texture, engineTextured2DGetByName("test_fixtures/test_texture.png"));

    destroy_file_scene();
    EXPECT_EQ(engineTextured2DGetByName("test_fixtures/test_texture.png"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(SceneFileTest, MissingModelOrMeshLeavesTheMeshWithout)
{
    create_file_scene();
    ASSERT_EQ(engineSceneLoad(write_file(R"({ "version": 1, "entities": [
        { "id": 1, "components": { "mesh": { "geometry": "test_fixtures/missing.gltf#Plate" } } },
        { "id": 2, "components": { "mesh": { "geometry": "test_fixtures/crate.gltf#NoSuchMesh" } } } ] })").c_str()), ENGINE_RESULT_CODE_OK);
    std::size_t meshes = 0;
    for (engine_game_object_t go = 1; go < 64; go++)
    {
        if (engineGameObjectIsValid(go) && engineGameObjectHasMeshComponent(go))
        {
            meshes++;
            EXPECT_EQ(engineGameObjectGetMeshComponent(go).geometry, ENGINE_INVALID_OBJECT_HANDLE);
        }
    }
    EXPECT_EQ(meshes, 2u);
}

TEST_F(SceneFileTest, FileWhichCantBeReadFailsTheLoad)
{
    EXPECT_EQ(engineSceneLoad((file_.string() + ".missing").c_str()), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(engineSceneLoad(write_file("not a scene").c_str()), ENGINE_RESULT_CODE_FAIL);
}

TEST_F(SceneFileTest, LoadedColliderIsHitAtItsWorldPosition)
{
    // a box under a parent moved to x = 5: saved, then loaded into another scene
    const auto parent = engineGameObjectCreate();
    auto parent_tc = engineGameObjectAddTransformComponent(parent);
    parent_tc.position[0] = 5.0f;
    engineGameObjectUpdateTransformComponent(parent, &parent_tc);
    const auto box = engineGameObjectCreate();
    auto box_tc = engineGameObjectAddTransformComponent(box);
    engineGameObjectUpdateTransformComponent(box, &box_tc);
    auto pc = engineGameObjectAddParentComponent(box);
    pc.parent = parent;
    engineGameObjectUpdateParentComponent(box, &pc);
    auto cc = engineGameObjectAddColliderComponent(box);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    cc.collider.box.size[0] = cc.collider.box.size[1] = cc.collider.box.size[2] = 0.5f;
    engineGameObjectUpdateColliderComponent(box, &cc);
    ASSERT_EQ(engineSceneSave(file_.string().c_str()), ENGINE_RESULT_CODE_OK);

    create_file_scene();
    ASSERT_EQ(engineSceneLoad(file_.string().c_str()), ENGINE_RESULT_CODE_OK);
    // a ray along x hits the box's near face at 4.5, not one at the origin (the ray's direction is its end point)
    const engine_ray_t ray{ { -10.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f } };
    const auto hit = enginePhysicsRayCast(nullptr, 0, &ray, 100.0f);
    ASSERT_NE(hit.go, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_NEAR(hit.position.x, 4.5f, 1e-3f);
    EXPECT_TRUE(engineGameObjectHasParentComponent(hit.go));
}

TEST_F(SceneFileTest, SavedSceneLoadsBackToTheSameFile)
{
    const auto go = engineGameObjectCreate();
    auto nc = engineGameObjectAddNameComponent(go);
    std::strcpy(nc.name, "lamp");
    engineGameObjectUpdateNameComponent(go, &nc);
    auto lc = engineGameObjectAddLightComponent(go);
    lc.type = ENGINE_LIGHT_TYPE_DIRECTIONAL;
    lc.directional.direction[1] = -1.0f;
    engineGameObjectUpdateLightComponent(go, &lc);
    // a transient object is not saved
    engineGameObjectAddTransformComponent(engineGameObjectCreateTransient());
    ASSERT_EQ(engineSceneSave(file_.string().c_str()), ENGINE_RESULT_CODE_OK);
    std::ifstream saved_file(file_, std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(saved_file)), std::istreambuf_iterator<char>());
    saved_file.close();
    EXPECT_EQ(saved.find("transform"), std::string::npos);

    create_file_scene();
    ASSERT_EQ(engineSceneLoad(file_.string().c_str()), ENGINE_RESULT_CODE_OK);
    const auto resaved = file_.string() + ".resaved";
    ASSERT_EQ(engineSceneSave(resaved.c_str()), ENGINE_RESULT_CODE_OK);
    std::ifstream resaved_file(resaved, std::ios::binary);
    const std::string again((std::istreambuf_iterator<char>(resaved_file)), std::istreambuf_iterator<char>());
    resaved_file.close();
    std::filesystem::remove(resaved);
    EXPECT_EQ(again, saved);
}
}  // namespace
