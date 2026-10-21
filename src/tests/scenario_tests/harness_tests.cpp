#include "scenario_harness.h"

#include <iscene.h>
#include <iscript.h>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

// Tests of the harness itself: the app runs without the editor, frames advance at a fixed time step,
// and the same scenario gives the same result every time.
namespace
{
using scenario::EmptyScene;

class UpdateCounter : public engine::IScript
{
public:
    UpdateCounter(engine::IScene* scene) : IScript(scene) {}
    void update(float dt) override
    {
        updates++;
        total_dt_ms += dt;
    }

    std::uint32_t updates = 0;
    float total_dt_ms = 0.0f;
};

// records the player input a gameplay script sees in its update
class InputProbe : public engine::IScript
{
public:
    InputProbe(engine::IScene* scene) : IScript(scene) {}
    void update(float) override
    {
        seen = static_cast<const project_c::AppProjectC*>(my_scene_->get_app())->player_input();
        frames_forward_held += seen.forward ? 1 : 0;
    }

    project_c::PlayerInput seen{};
    std::uint32_t frames_forward_held = 0;
};

engine_game_object_t create_falling_box(float height)
{
    const auto go = engineGameObjectCreate();

    auto tc = engineGameObjectAddTransformComponent(go);
    tc.position[1] = height;
    engineGameObjectUpdateTransformComponent(go, &tc);

    auto cc = engineGameObjectAddColliderComponent(go);
    cc.type = ENGINE_COLLIDER_TYPE_BOX;
    cc.collider.box.size[0] = 1.0f;
    cc.collider.box.size[1] = 1.0f;
    cc.collider.box.size[2] = 1.0f;
    engineGameObjectUpdateColliderComponent(go, &cc);

    auto rb = engineGameObjectAddRigidBodyComponent(go);
    rb.mass = 1.0f;
    engineGameObjectUpdateRigidBodyComponent(go, &rb);
    return go;
}

using HarnessTest = scenario::ScenarioTest;

TEST_F(HarnessTest, AppRunsWithoutEditor)
{
    EXPECT_FALSE(engineEditorIsEnabled());
    // without the editor gameplay is never paused
    EXPECT_TRUE(engineEditorIsPlaying());
}

TEST_F(HarnessTest, ScriptsTickEveryFrameWithFixedDeltaTime)
{
    auto* scene = load_scene<EmptyScene>();
    auto* counter = scene->register_script<UpdateCounter>();

    step(60);

    EXPECT_EQ(frame(), 60u);
    EXPECT_EQ(counter->updates, 60u);
    EXPECT_NEAR(counter->total_dt_ms, 1000.0f, 0.01f);
}

TEST_F(HarnessTest, ScriptsSeeInjectedInput)
{
    auto* scene = load_scene<EmptyScene>();
    auto* probe = scene->register_script<InputProbe>();

    project_c::PlayerInput input{};
    input.forward = true;
    input.attack = true;
    input.cursor = { 0.25f, 0.75f };
    set_input(input);
    step();

    // what the test set, not the real keyboard/mouse: the frame update doesn't overwrite it from the devices
    EXPECT_TRUE(probe->seen.forward);
    EXPECT_TRUE(probe->seen.attack);
    EXPECT_FALSE(probe->seen.dodge);
    EXPECT_FLOAT_EQ(probe->seen.cursor.x, 0.25f);
    EXPECT_FLOAT_EQ(probe->seen.cursor.y, 0.75f);

    release_all_input();
    step();

    EXPECT_FALSE(probe->seen.forward);
    EXPECT_FALSE(probe->seen.attack);
    EXPECT_FLOAT_EQ(probe->seen.cursor.x, 0.5f);
    EXPECT_FLOAT_EQ(probe->seen.cursor.y, 0.5f);
}

TEST_F(HarnessTest, InputIsHeldUntilChanged)
{
    auto* scene = load_scene<EmptyScene>();
    auto* probe = scene->register_script<InputProbe>();

    project_c::PlayerInput input{};
    input.forward = true;
    set_input(input);
    step(10);
    release_all_input();
    step(5);

    EXPECT_EQ(probe->frames_forward_held, 10u);
}

class FallingBoxTest : public scenario::ScenarioTest
{
protected:
    // a 1 kg box dropped from 10 m for one simulated second; returns its height
    float drop_box_for_one_second()
    {
        const auto box = create_falling_box(10.0f);
        step(60);
        return engineGameObjectGetTransformComponent(box).position[1];
    }
};

TEST_F(FallingBoxTest, FallsUnderGravity)
{
    load_scene<EmptyScene>();
    const auto height = drop_box_for_one_second();
    // gravity is 10 m/s^2: free fall covers ~5 m in 1 s
    EXPECT_NEAR(height, 5.0f, 0.5f);
}

TEST_F(FallingBoxTest, SameScenarioGivesSameResult)
{
    load_scene<EmptyScene>();
    const auto first = drop_box_for_one_second();
    TearDown();
    load_scene<EmptyScene>();
    const auto second = drop_box_for_one_second();
    // bit-identical, not just close: runs must be deterministic
    EXPECT_EQ(first, second);
}

}  // namespace

// The engine's frame time (not the harness, which always steps FIXED_DT_MS): a long frame (loading, a breakpoint) is
// reported as at most the app's max_delta_time, 1/3 s by default, so the game slows down instead of jumping ahead.
TEST(FrameTime, LongFrameIsCappedAtTheMaxDeltaTime)
{
    engineFrameBegin();
    engineFrameEnd();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    const auto frame = engineFrameBegin();
    engineFrameEnd();

    EXPECT_LE(frame.delta_time, 1000.0f / 3.0f + 0.01f);
    EXPECT_GT(frame.delta_time, 300.0f);
}
