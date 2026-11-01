#include "scenario_harness.h"
#include "test_models.h"

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Play mode through the C API (the harness's app has play_mode_control): Stop clears the active scene and loads back
// what Play saved of it.
namespace
{
// a scene file placing an instance of a test model, named `name`, with the persistent ID `id`
std::string placing(const char* model, const char* name, int id = 1)
{
    return std::string(R"({ "version": 1, "entities": [ { "id": )") + std::to_string(id) + R"(, "prefab": "test_fixtures/)" + model
        + R"(", "nodes": { "": { "components": { "name": { "name": ")" + name + R"(" } } } } } ] })";
}

// the scene the play session the next tests run in is started in
class PlaySessionScene : public engine::IScene
{
public:
    PlaySessionScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "play_mode_test_session"; }
};

class PlayModeTest : public scenario::ModelTest
{
protected:
    void SetUp() override
    {
        ModelTest::SetUp();
        // the app starts playing; the tests start in Edit mode, as the editor will
        stop();
    }

    void TearDown() override
    {
        // this test's play session is thrown away: its snapshot would be loaded into the next test's scene by its Stop
        if (enginePlayModeGetState() != ENGINE_PLAY_MODE_EDIT)
        {
            stop();
        }
        for (const auto& file : files_)
        {
            std::filesystem::remove(file);
        }
        ModelTest::TearDown();
        // the next tests play: the session starts in a scene of its own with nothing in it, so what it saves is empty
        scenario::app().register_scene<PlaySessionScene>()->activate();
        EXPECT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
        scenario::app().unregister_scene(PlaySessionScene::get_name());
    }

    // loads the scene file into the active scene, from a file outside the source tree's assets
    void load(const std::string& content)
    {
        const auto file = std::filesystem::temp_directory_path() / ("play_mode_test_" + std::to_string(files_.size()) + ".json");
        std::ofstream(file, std::ios::binary) << content;
        files_.push_back(file);
        ASSERT_EQ(engineSceneLoad(file.string().c_str()), ENGINE_RESULT_CODE_OK);
    }

    // the game objects of the active scene with this name
    static std::vector<engine_game_object_t> named(const char* name)
    {
        std::vector<engine_game_object_t> found;
        engine_component_view_t view{};
        engineComponentViewCreate(&view);
        engineComponentViewAttachNameComponent(view);
        engine_component_iterator_t it{};
        engine_component_iterator_t end{};
        engineComponentViewCreateBeginComponentIterator(view, &it);
        engineComponentViewCreateEndComponentIterator(view, &end);
        while (!engineComponentIteratorCheckEqual(it, end))
        {
            const auto go = engineComponentIteratorGetGameObject(it);
            if (std::strcmp(engineGameObjectGetNameComponent(go).name, name) == 0)
            {
                found.push_back(go);
            }
            engineComponentIteratorNext(it);
        }
        engineComponentIteratorDelete(it);
        engineComponentIteratorDelete(end);
        engineComponentViewDestroy(view);
        return found;
    }

    static engine_game_object_t create_named(const char* name, bool transient)
    {
        const auto go = transient ? engineGameObjectCreateTransient() : engineGameObjectCreate();
        auto nc = engineGameObjectAddNameComponent(go);
        std::strcpy(nc.name, name);
        engineGameObjectUpdateNameComponent(go, &nc);
        return go;
    }

private:
    std::vector<std::filesystem::path> files_;
};

TEST_F(PlayModeTest, StopGivesBackTheEditStateWithNewHandles)
{
    load(placing("crate.gltf", "crate"));
    const auto edit_crate = named("crate");
    ASSERT_EQ(edit_crate.size(), 1u);
    const auto wall = create_named("wall", false);
    auto transform = engineGameObjectAddTransformComponent(wall);
    transform.position[0] = 3.0f;
    engineGameObjectUpdateTransformComponent(wall, &transform);
    const auto sessions = enginePlayModeGetSessionCount();

    ASSERT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(enginePlayModeGetState(), ENGINE_PLAY_MODE_PLAYING);
    step(3);
    // playing: the wall moves, a runtime object is spawned, the crate is destroyed
    transform.position[0] = -10.0f;
    engineGameObjectUpdateTransformComponent(wall, &transform);
    create_named("coin", true);
    engineGameObjectDestroy(edit_crate[0]);
    step();

    // Stop: the session's last frame still has its game objects, the edit state comes back at its end
    ASSERT_EQ(enginePlayModeExit(), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(enginePlayModeGetState(), ENGINE_PLAY_MODE_EXITING);
    EXPECT_EQ(named("coin").size(), 1u);
    EXPECT_EQ(enginePlayModeGetSessionCount(), sessions);
    step();
    EXPECT_EQ(enginePlayModeGetState(), ENGINE_PLAY_MODE_EDIT);
    EXPECT_EQ(enginePlayModeGetSessionCount(), sessions + 1);
    EXPECT_TRUE(named("coin").empty());
    ASSERT_EQ(named("crate").size(), 1u);
    const auto walls = named("wall");
    ASSERT_EQ(walls.size(), 1u);
    EXPECT_FLOAT_EQ(engineGameObjectGetTransformComponent(walls[0]).position[0], 3.0f);
    // a handle kept from before Stop finds nothing, the restored game objects have new ones
    EXPECT_FALSE(engineGameObjectIsValid(wall));
    EXPECT_NE(walls[0], wall);
    EXPECT_FALSE(engineEditorIsPlaying());
}

TEST_F(PlayModeTest, StopReleasesOnlyTheAssetsThePlaySessionLoaded)
{
    // the scene holds the crate's model, the play session loads another one
    load(placing("crate.gltf", "crate"));
    const auto crate = enginePrefabGetByName("test_fixtures/crate.gltf");
    ASSERT_NE(crate, ENGINE_INVALID_OBJECT_HANDLE);
    ASSERT_EQ(enginePrefabGetByName("test_fixtures/textured_a.gltf"), ENGINE_INVALID_OBJECT_HANDLE);

    ASSERT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
    load(placing("textured_a.gltf", "textured", 2));
    ASSERT_NE(enginePrefabGetByName("test_fixtures/textured_a.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
    step();

    stop();
    EXPECT_EQ(enginePrefabGetByName("test_fixtures/textured_a.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
    // the same handle: the model both use was never released and loaded again
    EXPECT_EQ(enginePrefabGetByName("test_fixtures/crate.gltf"), crate);
}

TEST_F(PlayModeTest, StepPlaysOneFrameWhilePaused)
{
    ASSERT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
    ASSERT_EQ(enginePlayModeSetPaused(true), ENGINE_RESULT_CODE_OK);
    const auto playing_in_next_frame = []() {
        engineFrameBegin();
        const auto playing = engineEditorIsPlaying();
        engineFrameEnd();
        return playing;
    };
    EXPECT_FALSE(playing_in_next_frame());
    ASSERT_EQ(enginePlayModeStep(), ENGINE_RESULT_CODE_OK);
    EXPECT_TRUE(playing_in_next_frame());
    EXPECT_FALSE(playing_in_next_frame());
    EXPECT_EQ(enginePlayModeGetState(), ENGINE_PLAY_MODE_PAUSED);
}

// runs after the others (gtest keeps the order of definition): nothing an earlier test's play session saved is loaded
// into this test's scene by the Stop in SetUp
TEST_F(PlayModeTest, StartsWithNothingFromAnEarlierTest)
{
    EXPECT_TRUE(named("crate").empty());
    EXPECT_TRUE(named("wall").empty());
    EXPECT_EQ(enginePrefabGetByName("test_fixtures/crate.gltf"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(PlayModeTest, StopInEditModeIsRefused)
{
    const auto sessions = enginePlayModeGetSessionCount();
    EXPECT_EQ(enginePlayModeExit(), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(enginePlayModeSetPaused(true), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(enginePlayModeGetSessionCount(), sessions);
}
}  // namespace
