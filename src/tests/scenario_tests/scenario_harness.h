#pragma once
#include "app.h"

#include <iscene.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

namespace scenario
{
// one simulated frame; engine and scripts take delta time in milliseconds
inline constexpr float FIXED_DT_MS = 1000.0f / 60.0f;

// The application is created once for the whole test run (window, GL context, all prefabs) and shared by every test:
// hidden window, editor disabled (gameplay runs immediately), no level; the keyboard and mouse are never read because
// tests step frames themselves and only set the player input with set_input().
project_c::AppProjectC& app();
void create_app();
void destroy_app();

// A scene with nothing in it, for tests of the engine and the harness
class EmptyScene : public engine::IScene
{
public:
    EmptyScene(engine::IApplication* app) : IScene(app) {}
    static constexpr const char* get_name() { return "scenario_empty"; }
};

// Base fixture: load a scene with load_scene<T>(), then advance it with step(). The scene is unregistered after the test.
class ScenarioTest : public ::testing::Test
{
protected:
    // T derives from engine::IScene, is constructible from engine::IApplication* and has a static get_name()
    template<typename T>
    T* load_scene()
    {
        auto* scene = app().register_scene<T>();
        scene->activate();
        scene_name_ = T::get_name();
        frame_ = 0;
        return static_cast<T*>(scene);
    }

    // advances the game by `frames` frames of FIXED_DT_MS each, regardless of real time; runs the same
    // AppProjectC::update as the game's main loop
    void step(std::uint32_t frames = 1);

    // player input seen by gameplay from the next step() on; it stays until changed
    void set_input(const project_c::PlayerInput& input) { app().set_player_input(input); }
    void release_all_input() { app().set_player_input({}); }

    // frames stepped since the scene was loaded
    std::uint32_t frame() const { return frame_; }

    void TearDown() override;

private:
    std::string scene_name_;
    std::uint32_t frame_ = 0;
};
}  // namespace scenario
