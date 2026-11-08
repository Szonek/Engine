#include "scenario_harness.h"
#include "test_models.h"

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Particle effects through the C API, on the engine's test-fixture effects (assets/data/effects/test_fixtures): one-shot
// spawns and their removal, emitter components, play / stop, pause, play mode's Stop, and scene files.
namespace
{
constexpr const char* BURST = "effects/test_fixtures/burst.json";
constexpr const char* LOOPING = "effects/test_fixtures/looping.json";
constexpr const char* LIT_SMOKE = "effects/test_fixtures/lit_smoke.json";
// the generated puff flipbook lit_smoke.json draws (assets/models/particles)
constexpr const char* SMOKE_PUFF = "particles/smoke_puff.png";
constexpr float IDENTITY[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

// a test-fixture effect, released with the test
class TestEffect
{
public:
    explicit TestEffect(const char* path) : asset_(engineDataAssetLoad(path)) {}
    TestEffect(const TestEffect&) = delete;
    TestEffect& operator=(const TestEffect&) = delete;
    ~TestEffect()
    {
        if (asset_ != ENGINE_INVALID_OBJECT_HANDLE)
        {
            engineDataAssetRelease(asset_);
        }
    }
    engine_data_asset_t get() const { return asset_; }

private:
    engine_data_asset_t asset_;
};

class ParticleTest : public scenario::ModelTest
{
protected:
    // a game object at the origin playing `effect`
    static engine_game_object_t emitter(engine_data_asset_t effect, bool play_on_start = true)
    {
        const auto go = engineGameObjectCreate();
        engineGameObjectAddTransformComponent(go);
        auto emitter = engineGameObjectAddParticleEmitterComponent(go);
        emitter.effect = effect;
        emitter.play_on_start = play_on_start;
        engineGameObjectUpdateParticleEmitterComponent(go, &emitter);
        return go;
    }

    static void set_name(engine_game_object_t go, const char* name)
    {
        auto nc = engineGameObjectAddNameComponent(go);
        std::strcpy(nc.name, name);
        engineGameObjectUpdateNameComponent(go, &nc);
    }

    // the game object of the active scene with this name, ENGINE_INVALID_GAME_OBJECT_ID when none has it
    static engine_game_object_t named(const char* name)
    {
        engine_game_object_t found = ENGINE_INVALID_GAME_OBJECT_ID;
        engine_component_view_t view{};
        engineComponentViewCreate(&view);
        engineComponentViewAttachNameComponent(view);
        engine_component_iterator_t it{};
        engine_component_iterator_t end{};
        engineComponentViewCreateBeginComponentIterator(view, &it);
        engineComponentViewCreateEndComponentIterator(view, &end);
        while (!engineComponentIteratorCheckEqual(it, end))
        {
            const auto candidate = engineComponentIteratorGetGameObject(it);
            if (std::strcmp(engineGameObjectGetNameComponent(candidate).name, name) == 0)
            {
                found = candidate;
            }
            engineComponentIteratorNext(it);
        }
        engineComponentIteratorDelete(it);
        engineComponentIteratorDelete(end);
        engineComponentViewDestroy(view);
        return found;
    }
};
}  // namespace

TEST_F(ParticleTest, NewEmitterComponentHasTheDefaults)
{
    const auto go = engineGameObjectCreate();
    const auto emitter = engineGameObjectAddParticleEmitterComponent(go);
    EXPECT_EQ(emitter.effect, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_TRUE(emitter.play_on_start);
    EXPECT_EQ(emitter.scale, 1.0f);
    EXPECT_EQ(emitter.seed, 0u);
    EXPECT_TRUE(engineGameObjectHasParticleEmitterComponent(go));
    // nothing to play
    step(2);
    EXPECT_FALSE(engineParticlesIsAlive(go));
    EXPECT_EQ(engineParticlesGetCount(go), 0u);
}

TEST_F(ParticleTest, SpawnedBurstEmitsItsCountAndIsDestroyedWhenItDies)
{
    TestEffect burst(BURST);
    ASSERT_NE(burst.get(), ENGINE_INVALID_OBJECT_HANDLE);
    const auto go = engineParticlesSpawn(burst.get(), IDENTITY, nullptr);
    ASSERT_NE(go, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_TRUE(engineGameObjectIsTransient(go));
    EXPECT_TRUE(engineParticlesIsAlive(go));
    step();
    EXPECT_EQ(engineParticlesGetCount(go), 10u);
    // its particles live 0.5 s; then the engine destroys its game object
    step(20);
    EXPECT_EQ(engineParticlesGetCount(go), 10u);
    step(20);
    EXPECT_FALSE(engineGameObjectIsValid(go));
}

TEST_F(ParticleTest, SpawnOfSomethingElseThanAnEffectFails)
{
    EXPECT_EQ(engineParticlesSpawn(ENGINE_INVALID_OBJECT_HANDLE, IDENTITY, nullptr), ENGINE_INVALID_GAME_OBJECT_ID);
    TestEffect burst(BURST);
    EXPECT_EQ(engineParticlesSpawn(burst.get(), nullptr, nullptr), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(ParticleTest, SpawnParamsScaleAndSeed)
{
    TestEffect burst(BURST);
    engine_particles_spawn_params_t params{};
    params.scale = 2.0f;
    params.seed = 5;
    params.tint[0] = params.tint[1] = params.tint[2] = params.tint[3] = 1.0f;
    const auto go = engineParticlesSpawn(burst.get(), IDENTITY, &params);
    ASSERT_NE(go, ENGINE_INVALID_GAME_OBJECT_ID);
    const auto emitter = engineGameObjectGetParticleEmitterComponent(go);
    EXPECT_EQ(emitter.scale, 2.0f);
    EXPECT_EQ(emitter.seed, 5u);
    EXPECT_EQ(emitter.effect, burst.get());
    step();
    EXPECT_EQ(engineParticlesGetCount(go), 10u);
}

TEST_F(ParticleTest, EmitterPlaysOnStartOrWaitsForPlay)
{
    TestEffect looping(LOOPING);
    const auto playing = emitter(looping.get(), true);
    const auto waiting = emitter(looping.get(), false);
    step(30);
    EXPECT_GT(engineParticlesGetCount(playing), 20u);
    EXPECT_EQ(engineParticlesGetCount(waiting), 0u);
    EXPECT_FALSE(engineParticlesIsAlive(waiting));

    ASSERT_EQ(engineParticlesPlay(waiting), ENGINE_RESULT_CODE_OK);
    step(30);
    EXPECT_GT(engineParticlesGetCount(waiting), 20u);
    // a component placed in the level stays when its effect stops
    ASSERT_EQ(engineParticlesStop(waiting, ENGINE_PARTICLES_STOP_MODE_CLEAR), ENGINE_RESULT_CODE_OK);
    step(120);
    EXPECT_TRUE(engineGameObjectIsValid(waiting));
    EXPECT_FALSE(engineParticlesIsAlive(waiting));
}

TEST_F(ParticleTest, StopLetsParticlesLiveOutOrClearsThem)
{
    TestEffect looping(LOOPING);
    const auto live_out = emitter(looping.get());
    const auto clear = emitter(looping.get());
    step(30);
    const auto before = engineParticlesGetCount(live_out);
    ASSERT_GT(before, 0u);
    ASSERT_EQ(engineParticlesStop(live_out, ENGINE_PARTICLES_STOP_MODE_LIVE_OUT), ENGINE_RESULT_CODE_OK);
    ASSERT_EQ(engineParticlesStop(clear, ENGINE_PARTICLES_STOP_MODE_CLEAR), ENGINE_RESULT_CODE_OK);
    EXPECT_EQ(engineParticlesGetCount(clear), 0u);
    step(10);
    EXPECT_GT(engineParticlesGetCount(live_out), 0u);
    EXPECT_LE(engineParticlesGetCount(live_out), before);
    // their lifetime is 1 s
    step(60);
    EXPECT_EQ(engineParticlesGetCount(live_out), 0u);
    EXPECT_FALSE(engineParticlesIsAlive(live_out));

    // no emitter component: nothing to play or stop
    const auto other = engineGameObjectCreate();
    EXPECT_EQ(engineParticlesPlay(other), ENGINE_RESULT_CODE_FAIL);
    EXPECT_EQ(engineParticlesStop(other, ENGINE_PARTICLES_STOP_MODE_CLEAR), ENGINE_RESULT_CODE_FAIL);
}

TEST_F(ParticleTest, PauseFreezesParticles)
{
    TestEffect looping(LOOPING);
    const auto go = emitter(looping.get());
    step(30);
    const auto before = engineParticlesGetCount(go);
    ASSERT_GT(before, 0u);
    ASSERT_EQ(enginePlayModeSetPaused(true), ENGINE_RESULT_CODE_OK);
    step(30);
    EXPECT_EQ(engineParticlesGetCount(go), before);
    ASSERT_EQ(enginePlayModeSetPaused(false), ENGINE_RESULT_CODE_OK);
    step(30);
    EXPECT_GT(engineParticlesGetCount(go), before);
}

TEST_F(ParticleTest, RemovingTheComponentDropsTheParticles)
{
    TestEffect looping(LOOPING);
    const auto go = emitter(looping.get());
    step(10);
    ASSERT_GT(engineParticlesGetCount(go), 0u);
    engineGameObjectRemoveParticleEmitterComponent(go);
    EXPECT_EQ(engineParticlesGetCount(go), 0u);
    EXPECT_FALSE(engineParticlesIsAlive(go));
    step();
}

TEST_F(ParticleTest, EmitterComponentRoundTripsThroughAFile)
{
    TestEffect burst(BURST);
    const auto go = emitter(burst.get(), false);
    set_name(go, "torch");
    auto component = engineGameObjectGetParticleEmitterComponent(go);
    component.scale = 2.5f;
    component.seed = 7;
    engineGameObjectUpdateParticleEmitterComponent(go, &component);
    const auto file = std::filesystem::temp_directory_path() / "project_c_particle_emitter_test.json";
    ASSERT_EQ(engineSceneSave(file.string().c_str()), ENGINE_RESULT_CODE_OK);
    std::stringstream text;
    text << std::ifstream(file).rdbuf();
    EXPECT_NE(text.str().find("\"particle_emitter\""), std::string::npos) << text.str();
    EXPECT_NE(text.str().find(BURST), std::string::npos) << text.str();

    // loaded again in its place
    engineGameObjectDestroy(go);
    step();
    ASSERT_EQ(named("torch"), ENGINE_INVALID_GAME_OBJECT_ID);
    ASSERT_EQ(engineSceneLoad(file.string().c_str()), ENGINE_RESULT_CODE_OK);
    std::filesystem::remove(file);
    const auto loaded = named("torch");
    ASSERT_NE(loaded, ENGINE_INVALID_GAME_OBJECT_ID);
    ASSERT_TRUE(engineGameObjectHasParticleEmitterComponent(loaded));
    const auto read = engineGameObjectGetParticleEmitterComponent(loaded);
    EXPECT_EQ(read.effect, burst.get());
    EXPECT_FALSE(read.play_on_start);
    EXPECT_EQ(read.scale, 2.5f);
    EXPECT_EQ(read.seed, 7u);
    // what it plays isn't saved: the loaded one waits for Play
    step();
    EXPECT_EQ(engineParticlesGetCount(loaded), 0u);
    ASSERT_EQ(engineParticlesPlay(loaded), ENGINE_RESULT_CODE_OK);
    step();
    EXPECT_EQ(engineParticlesGetCount(loaded), 10u);
}

// Play mode's Stop gives back the scene as it was at Play: the emitter, without the particles of the play session
namespace
{
class ParticlePlaySessionScene : public engine::IScene
{
public:
    ParticlePlaySessionScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "particle_test_session"; }
};

class ParticlePlayModeTest : public ParticleTest
{
protected:
    void SetUp() override
    {
        ParticleTest::SetUp();
        // the tests start in Edit mode, as the editor does
        stop();
    }
    void TearDown() override
    {
        if (enginePlayModeGetState() != ENGINE_PLAY_MODE_EDIT)
        {
            stop();
        }
        ParticleTest::TearDown();
        // the next tests play, in a session started in an empty scene of its own (see PlayModeTest)
        scenario::app().register_scene<ParticlePlaySessionScene>()->activate();
        EXPECT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
        scenario::app().unregister_scene(ParticlePlaySessionScene::get_name());
    }
};
}  // namespace

TEST_F(ParticlePlayModeTest, EditModeFreezesAndStopClearsThePlaySessionsParticles)
{
    TestEffect looping(LOOPING);
    const auto go = emitter(looping.get());
    set_name(go, "fountain");
    // in Edit mode the scene steps with dt 0: nothing is emitted
    step(30);
    EXPECT_EQ(engineParticlesGetCount(go), 0u);

    ASSERT_EQ(enginePlayModeEnter(), ENGINE_RESULT_CODE_OK);
    step(30);
    EXPECT_GT(engineParticlesGetCount(go), 0u);
    const auto spawned = engineParticlesSpawn(looping.get(), IDENTITY, nullptr);
    ASSERT_NE(spawned, ENGINE_INVALID_GAME_OBJECT_ID);
    step();

    stop();
    // the emitter is back (a new handle), without particles; the spawned one-shot is gone
    EXPECT_FALSE(engineGameObjectIsValid(go));
    EXPECT_FALSE(engineGameObjectIsValid(spawned));
    const auto restored = named("fountain");
    ASSERT_NE(restored, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_TRUE(engineGameObjectHasParticleEmitterComponent(restored));
    EXPECT_EQ(engineParticlesGetCount(restored), 0u);
}

// The lit soft smoke (lit, soft, eroding, a flipbook texture) is drawn by a camera with a shadowed directional light over
// a floor: the effect loads its texture as a dependency, plays and keeps the texture while it is loaded; the frames
// render its Transparent pass with the scene depth copy and the shadow map (a shader that doesn't compile is logged).
TEST_F(ParticleTest, LitSoftSmokeDrawsWithItsFlipbookTexture)
{
    scenario::TestModel crate("crate.gltf");
    ASSERT_TRUE(crate.loaded());
    const auto floor = enginePrefabInstantiate(crate.prefab());
    auto floor_transform = engineGameObjectGetTransformComponent(floor);
    floor_transform.position[1] = -0.5f;
    floor_transform.scale[0] = 10.0f;
    floor_transform.scale[2] = 10.0f;
    engineGameObjectUpdateTransformComponent(floor, &floor_transform);

    const auto camera = engineGameObjectCreate();
    auto cc = engineGameObjectAddCameraComponent(camera);
    cc.enabled = true;
    cc.target[1] = 1.0f;
    engineGameObjectUpdateCameraComponent(camera, &cc);
    auto camera_transform = engineGameObjectAddTransformComponent(camera);
    camera_transform.position[1] = 3.0f;
    camera_transform.position[2] = -8.0f;
    engineGameObjectUpdateTransformComponent(camera, &camera_transform);

    const auto light = engineGameObjectCreate();
    auto lc = engineGameObjectAddLightComponent(light);
    lc.type = ENGINE_LIGHT_TYPE_DIRECTIONAL;
    lc.intensity.ambient[0] = lc.intensity.ambient[1] = lc.intensity.ambient[2] = 0.3f;
    lc.intensity.diffuse[0] = lc.intensity.diffuse[1] = lc.intensity.diffuse[2] = 0.8f;
    lc.directional.direction[0] = 0.4f;
    lc.directional.direction[1] = 1.0f;
    lc.directional.direction[2] = 0.3f;
    engineGameObjectUpdateLightComponent(light, &lc);

    {
        TestEffect smoke(LIT_SMOKE);
        ASSERT_NE(smoke.get(), ENGINE_INVALID_OBJECT_HANDLE);
        EXPECT_NE(engineTextured2DGetByName(SMOKE_PUFF), ENGINE_INVALID_OBJECT_HANDLE);
        const auto go = emitter(smoke.get());
        // two seconds: about 24 puffs, the first ones eroding
        step(120);
        EXPECT_TRUE(engineParticlesIsAlive(go));
        EXPECT_GT(engineParticlesGetCount(go), 15u);
        engineGameObjectDestroy(go);
        step();
    }
    // the texture went with the effect
    EXPECT_EQ(engineTextured2DGetByName(SMOKE_PUFF), ENGINE_INVALID_OBJECT_HANDLE);
}
