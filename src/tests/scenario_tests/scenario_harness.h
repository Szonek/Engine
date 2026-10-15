#pragma once
#include "app.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

namespace scenario
{
// one simulated frame; engine and scripts take delta time in milliseconds
inline constexpr float FIXED_DT_MS = 1000.0f / 60.0f;

// What the player is pressing, in game terms; ScenarioTest maps it to the keys/buttons the game reads.
// Scripts detect presses on the up -> down change, so holding `attack` for several frames is a single attack.
struct PlayerInput
{
    bool forward = false;      // W
    bool back = false;         // S
    bool left = false;         // A
    bool right = false;        // D
    bool attack = false;       // right mouse button
    bool dodge = false;        // space
    bool interact = false;     // left mouse button
    bool drop_weapon = false;  // F
    // cursor, as engineMouseCoordsGet reports it: (0, 0) bottom-left of the window, (1, 1) top-right
    float cursor_x = 0.5f;
    float cursor_y = 0.5f;
};

// The application is created once for the whole test run (window, GL context, all prefabs) and shared by every test:
// hidden window, editor disabled (gameplay runs immediately), no default level.
project_c::AppProjectC& app();
void create_app();
void destroy_app();

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

    // advances the loaded scene by `frames` frames of FIXED_DT_MS each, regardless of real time
    void step(std::uint32_t frames = 1);

    // input seen by the game from the next step() on; it stays until changed. Real keyboard/mouse are never read.
    void set_input(const PlayerInput& input) { input_ = input; }
    void release_all_input() { input_ = {}; }

    // frames stepped since the scene was loaded
    std::uint32_t frame() const { return frame_; }

    void TearDown() override;

private:
    void apply_input() const;

    std::string scene_name_;
    std::uint32_t frame_ = 0;
    PlayerInput input_{};
};
}  // namespace scenario
