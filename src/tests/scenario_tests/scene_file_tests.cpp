#include "scenario_harness.h"
#include "test_models.h"

#include <iscene.h>

#include <gtest/gtest.h>

#include <cstddef>
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

// a game-defined component; types can't be unregistered, so it is registered once for the whole run
struct ScenarioTag
{
    float weight;
    engine_game_object_t buddy;
    char label[8];
};

engine_component_type_t scenario_tag_type()
{
    static const engine_component_type_t type = []
    {
        engine_field_desc_t fields[3]{};
        fields[0] = { "weight", ENGINE_FIELD_KIND_FLOAT, offsetof(ScenarioTag, weight) };
        fields[1] = { "buddy", ENGINE_FIELD_KIND_GAME_OBJECT, offsetof(ScenarioTag, buddy) };
        fields[2] = { "label", ENGINE_FIELD_KIND_STRING, offsetof(ScenarioTag, label), sizeof(ScenarioTag::label) };
        const ScenarioTag defaults{ 2.0f, ENGINE_INVALID_GAME_OBJECT_ID, "tag" };
        const engine_type_desc_t desc{ "scenario_tag", 1, sizeof(ScenarioTag), alignof(ScenarioTag), &defaults, fields, 3 };
        engine_component_type_t out = ENGINE_INVALID_OBJECT_HANDLE;
        engineComponentTypeRegister(&desc, &out);
        return out;
    }();
    return type;
}

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
    // a ray along x hits the box's near face at 4.5, not one at the origin
    const float origin[3] = { -10.0f, 0.0f, 0.0f };
    const float along_x[3] = { 1.0f, 0.0f, 0.0f };
    engine_ray_hit_t hit{};
    ASSERT_TRUE(enginePhysicsRayCast(origin, along_x, 100.0f, nullptr, &hit));
    EXPECT_NEAR(hit.position[0], 4.5f, 1e-3f);
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

TEST_F(SceneFileTest, GameComponentWorksLikeTheEngineComponents)
{
    const auto type = scenario_tag_type();
    ASSERT_NE(type, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineComponentTypeGetByName("scenario_tag"), type);
    EXPECT_NE(engineComponentTypeGetByName("transform"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineComponentTypeGetByName("no_such_type"), ENGINE_INVALID_OBJECT_HANDLE);

    const auto go = engineGameObjectCreate();
    ScenarioTag tag{};
    EXPECT_FALSE(engineGameObjectHasDataComponent(go, type));
    EXPECT_EQ(engineGameObjectGetDataComponent(go, type, &tag), ENGINE_RESULT_CODE_FAIL);
    // added with the type's default value
    ASSERT_EQ(engineGameObjectAddDataComponent(go, type, nullptr), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(engineGameObjectAddDataComponent(go, type, nullptr), ENGINE_RESULT_CODE_FAIL);
    ASSERT_EQ(engineGameObjectGetDataComponent(go, type, &tag), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(tag.weight, 2.0f);
    EXPECT_STREQ(tag.label, "tag");

    tag.weight = 5.0f;
    ASSERT_EQ(engineGameObjectUpdateDataComponent(go, type, &tag), ENGINE_RESULT_CODE_OK);
    ScenarioTag read{};
    engineGameObjectGetDataComponent(go, type, &read);
    EXPECT_EQ(read.weight, 5.0f);

    engineGameObjectRemoveDataComponent(go, type);
    EXPECT_FALSE(engineGameObjectHasDataComponent(go, type));
    EXPECT_EQ(engineGameObjectUpdateDataComponent(go, type, &tag), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(engineGameObjectAddDataComponent(go, ENGINE_INVALID_OBJECT_HANDLE, nullptr), ENGINE_RESULT_CODE_INVALID_ARG);
    EXPECT_EQ(engineGameObjectAddDataComponent(ENGINE_INVALID_GAME_OBJECT_ID, type, nullptr), ENGINE_RESULT_CODE_INVALID_ARG);

    // destroyed with its game object
    engineGameObjectAddDataComponent(go, type, &tag);
    engineGameObjectDestroy(go);
    EXPECT_FALSE(engineGameObjectHasDataComponent(go, type));
}

TEST_F(SceneFileTest, BadComponentTypeIsNotRegistered)
{
    // the field is outside the struct
    const engine_field_desc_t field{ "weight", ENGINE_FIELD_KIND_FLOAT, sizeof(ScenarioTag) };
    const engine_type_desc_t desc{ "scenario_bad_tag", 1, sizeof(ScenarioTag), alignof(ScenarioTag), nullptr, &field, 1 };
    engine_component_type_t out = 0;
    EXPECT_EQ(engineComponentTypeRegister(&desc, &out), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(out, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(engineComponentTypeGetByName("scenario_bad_tag"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(SceneFileTest, GameComponentIsSavedAndLoaded)
{
    const auto type = scenario_tag_type();
    const auto buddy = engineGameObjectCreate();
    engineGameObjectAddTransformComponent(buddy);
    const auto go = engineGameObjectCreate();
    const ScenarioTag tag{ 0.25f, buddy, "saved" };
    ASSERT_EQ(engineGameObjectAddDataComponent(go, type, &tag), ENGINE_RESULT_CODE_OK);
    ASSERT_EQ(engineSceneSave(file_.string().c_str()), ENGINE_RESULT_CODE_OK);

    create_file_scene();
    ASSERT_EQ(engineSceneLoad(file_.string().c_str()), ENGINE_RESULT_CODE_OK);
    engine_game_object_t loaded = ENGINE_INVALID_GAME_OBJECT_ID;
    for (engine_game_object_t candidate = 1; candidate < 64; candidate++)
    {
        if (engineGameObjectIsValid(candidate) && engineGameObjectHasDataComponent(candidate, type))
        {
            loaded = candidate;
        }
    }
    ASSERT_NE(loaded, ENGINE_INVALID_GAME_OBJECT_ID);
    ScenarioTag read{};
    ASSERT_EQ(engineGameObjectGetDataComponent(loaded, type, &read), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(read.weight, 0.25f);
    EXPECT_STREQ(read.label, "saved");
    // the reference points at the loaded buddy
    ASSERT_TRUE(engineGameObjectIsValid(read.buddy));
    EXPECT_TRUE(engineGameObjectHasTransformComponent(read.buddy));
}

TEST_F(SceneFileTest, SceneSavedToMemoryLoadsBackToTheSameContent)
{
    const auto go = engineGameObjectCreate();
    auto nc = engineGameObjectAddNameComponent(go);
    std::strcpy(nc.name, "snapshot");
    engineGameObjectUpdateNameComponent(go, &nc);
    const ScenarioTag tag{ 1.5f, go, "self" };
    ASSERT_EQ(engineGameObjectAddDataComponent(go, scenario_tag_type(), &tag), ENGINE_RESULT_CODE_OK);
    auto* saved = engineStringCreate("");
    ASSERT_EQ(engineSceneSaveToMemory(saved), ENGINE_RESULT_CODE_OK);
    const std::string content = engineStringCStr(saved);
    EXPECT_NE(content.find(R"("snapshot")"), std::string::npos);
    EXPECT_NE(content.find(R"("scenario_tag")"), std::string::npos);

    create_file_scene();
    ASSERT_EQ(engineSceneLoadFromMemory(content.data(), content.size(), "snapshot"), ENGINE_RESULT_CODE_OK);
    // the content is replaced, not appended to
    engineStringSet(saved, "old content");
    ASSERT_EQ(engineSceneSaveToMemory(saved), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(std::string(engineStringCStr(saved)), content);
    engineStringDestroy(saved);
}

TEST_F(SceneFileTest, ContentWhichIsNotASceneFailsTheLoadFromMemory)
{
    const std::string content = "not a scene";
    EXPECT_EQ(engineSceneLoadFromMemory(content.data(), content.size(), nullptr), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(engineSceneLoadFromMemory(nullptr, 0, nullptr), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(engineSceneLoadFromMemory(nullptr, 4, nullptr), ENGINE_RESULT_CODE_INVALID_ARG);
    EXPECT_EQ(engineSceneSaveToMemory(nullptr), ENGINE_RESULT_CODE_INVALID_ARG);
}
}  // namespace
