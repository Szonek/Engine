#include <engine.h>
#include <iostream>
#include <format>


int main(int argc, char** argv)
{
	std::string assets_path = "";
	if (argc > 1)
	{
		assets_path = argv[1];
	}
	std::cout << std::format("Reading assets from path: {}\n", assets_path);

	engine_application_t app{};
	engine_application_create_desc_t app_cd{};
	app_cd.name = "LeanrOpengl";
	app_cd.asset_store_path = assets_path.c_str();
	app_cd.width = 800;
	app_cd.height = 600;
	auto engine_error_code = engineApplicationCreate(&app, app_cd);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineApplicationDestroy(app);
		std::cout << std::format("Couldnt create engine application!\n");
		return -1;
	}
	
	bool run_main_loop = true;
	while (run_main_loop)
	{
		const auto frame_begin = engineApplicationFrameBegine(app);

		engineApplicationFrameRunScene(app, nullptr, frame_begin.delta_time);

		const auto frame_end = engineApplicationFrameEnd(app);
		run_main_loop = frame_end.success;
	}

	engineApplicationDestroy(app);
	return 0;
}