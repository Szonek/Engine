#include "scenario_harness.h"

#include <iscene.h>
#include <iscript.h>

#include <gtest/gtest.h>

// Tests of the harness itself: the app runs without the editor, frames advance at a fixed time step,
// and the same scenario gives the same result every time.
namespace
{
class EmptyScene : public engine::IScene
{
public:
    EmptyScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "scenario_empty"; }
};

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

// records what a gameplay script reads from the input API in its update
class InputProbe : public engine::IScript
{
public:
    InputProbe(engine::IScene* scene) : IScript(scene) {}
    void update(float) override
    {
        forward = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_W);
        dodge = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_SPACE);
        escape = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_ESCAPE);
        attack = engineMouseIsButtonDown(ENGINE_MOUSE_BUTTON_RIGHT);
        cursor = engineMouseCoordsGet();
        frames_forward_held += forward ? 1 : 0;
    }

    bool forward = false;
    bool dodge = false;
    bool escape = false;
    bool attack = false;
    engine_fvec2_t cursor{};
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

    scenario::PlayerInput input{};
    input.forward = true;
    input.attack = true;
    input.cursor_x = 0.25f;
    input.cursor_y = 0.75f;
    set_input(input);
    step();

    EXPECT_TRUE(probe->forward);
    EXPECT_TRUE(probe->attack);
    EXPECT_FALSE(probe->dodge);
    // keys the harness doesn't drive stay up, whatever the real keyboard does
    EXPECT_FALSE(probe->escape);
    EXPECT_FLOAT_EQ(probe->cursor.x, 0.25f);
    EXPECT_FLOAT_EQ(probe->cursor.y, 0.75f);

    release_all_input();
    step();

    EXPECT_FALSE(probe->forward);
    EXPECT_FALSE(probe->attack);
    EXPECT_FLOAT_EQ(probe->cursor.x, 0.5f);
    EXPECT_FLOAT_EQ(probe->cursor.y, 0.5f);
}

TEST_F(HarnessTest, InputIsHeldUntilChanged)
{
    auto* scene = load_scene<EmptyScene>();
    auto* probe = scene->register_script<InputProbe>();

    scenario::PlayerInput input{};
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
