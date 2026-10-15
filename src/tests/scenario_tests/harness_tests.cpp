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
