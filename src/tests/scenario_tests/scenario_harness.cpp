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
    // the play mode tests stop and start play sessions; every other test plays, as the app starts playing
    config.play_mode_control = true;
    // small, so rendering costs little per simulated frame
    config.window_width = 320;
    config.window_height = 180;
    G_APP = std::make_unique<project_c::AppProjectC>(project_c::default_prefabs_data(), config);
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
        app().update(FIXED_DT_MS);
        engineFrameEnd();
        frame_++;
    }
}

void scenario::ScenarioTest::TearDown()
{
    release_all_input();
    if (!scene_name_.empty())
    {
        app().unregister_scene(scene_name_);
        scene_name_.clear();
    }
}
