#include <engine.h>

#include "app.h"
#include "scenes/scene_test.h"
#include "scenes/scene_city.h"

int main(int argc, char** argv)
{
    try
    {
        project_c::AppProjectC app_project_c;

        app_project_c.register_scene<project_c::CityScene>();
        app_project_c.register_scene<project_c::TestScene>();

        app_project_c.run();

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