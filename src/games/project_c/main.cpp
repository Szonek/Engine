#include <engine.h>

#include "app.h"
#include "player_input.h"
#include "scenes/level_scene.h"

#include <string_view>

namespace
{
// The game's main loop: platform concerns only (frame timing, quitting, reading the devices, FPS log).
// Game rules live in AppProjectC::update, which scenario tests drive the same way with a fixed time step.
void run(project_c::AppProjectC& app)
{
    struct fps_counter_t
    {
        float frames_total_time = 0.0f;
        std::uint32_t frames_count = 0;
    };
    fps_counter_t fps_counter{};

    while (true)
    {
        const auto frame_begin = engineFrameBegin();

        if (frame_begin.events & ENGINE_EVENT_QUIT)
        {
            log(fmt::format("Engine requested app quit. Exiting.\n"));
            break;
        }

        const auto input = project_c::read_player_input_from_devices();
        if (input.quit)
        {
            log(fmt::format("User requested quit. Exiting.\n"));
            break;
        }

        fps_counter.frames_count += 1;
        fps_counter.frames_total_time += frame_begin.delta_time;
        if (fps_counter.frames_total_time > 1000.0f)
        {
            log(fmt::format("FPS: {}, latency: {} ms. \n",
                fps_counter.frames_count, fps_counter.frames_total_time / fps_counter.frames_count));
            fps_counter = {};
        }

        app.set_player_input(input);
        app.update(frame_begin.delta_time);

        const auto frame_end = engineFrameEnd();
        if (!frame_end.success)
        {
            log(fmt::format("Frame not finished sucesfully. Exiting.\n"));
            break;
        }
    }
}

// the options of the command line: --level <path> (a level file, relative to assets/scenes or absolute)
project_c::AppConfig read_options(int argc, char** argv)
{
    project_c::AppConfig config{};
    for (int i = 1; i < argc; i++)
    {
        const std::string_view option = argv[i];
        if (option == "--level" && i + 1 < argc)
        {
            config.level_path = argv[++i];
        }
        else
        {
            log(fmt::format("Unknown option {} (the options: --level <path>).\n", option));
        }
    }
    return config;
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        project_c::AppProjectC app_project_c(project_c::default_prefabs_data(), read_options(argc, argv));
        app_project_c.register_scene<project_c::LevelScene>()->activate();
        run(app_project_c);

        return 0;
    }
    catch (const std::exception& e)
    {
        log(fmt::format("Exception: {}\n", e.what()));
        return -1;
    }
    catch (...)
    {
        log("Unknown exception\n");
        return -2;
    }
	return 0;
}