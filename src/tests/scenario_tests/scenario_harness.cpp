#include "scenario_harness.h"

namespace
{
std::unique_ptr<project_c::AppProjectC> G_APP;
}  // namespace

project_c::AppProjectC& scenario::app()
{
    if (!G_APP)
    {
        throw std::runtime_error("Scenario app not created");
    }
    return *G_APP;
}

void scenario::create_app()
{
    project_c::AppConfig config{};
    config.enable_editor = false;
    config.hidden_window = true;
    config.register_default_scene = false;
    // small, so rendering costs little per simulated frame
    config.window_width = 320;
    config.window_height = 180;
    G_APP = std::make_unique<project_c::AppProjectC>(project_c::default_prefabs_data(), config);
    // tests drive the game only through ScenarioTest::set_input
    engineInputInjectionSetEnabled(true);
}

void scenario::destroy_app()
{
    G_APP.reset();
}

void scenario::ScenarioTest::step(std::uint32_t frames)
{
    for (std::uint32_t i = 0; i < frames; i++)
    {
        // the measured frame time is ignored: every frame simulates exactly FIXED_DT_MS
        engineFrameBegin();
        apply_input();
        app().update_scenes(FIXED_DT_MS);
        engineFrameEnd();
        frame_++;
    }
}

void scenario::ScenarioTest::apply_input() const
{
    engineInputInjectKeyboardKey(ENGINE_KEYBOARD_KEY_W, input_.forward);
    engineInputInjectKeyboardKey(ENGINE_KEYBOARD_KEY_S, input_.back);
    engineInputInjectKeyboardKey(ENGINE_KEYBOARD_KEY_A, input_.left);
    engineInputInjectKeyboardKey(ENGINE_KEYBOARD_KEY_D, input_.right);
    engineInputInjectKeyboardKey(ENGINE_KEYBOARD_KEY_SPACE, input_.dodge);
    engineInputInjectKeyboardKey(ENGINE_KEYBOARD_KEY_F, input_.drop_weapon);
    engineInputInjectMouseButton(ENGINE_MOUSE_BUTTON_RIGHT, input_.attack);
    engineInputInjectMouseButton(ENGINE_MOUSE_BUTTON_LEFT, input_.interact);
    engineInputInjectMouseCoords(engine_fvec2_t{ input_.cursor_x, input_.cursor_y });
}

void scenario::ScenarioTest::TearDown()
{
    release_all_input();
    apply_input();
    if (!scene_name_.empty())
    {
        app().unregister_scene(scene_name_);
        scene_name_.clear();
    }
}
