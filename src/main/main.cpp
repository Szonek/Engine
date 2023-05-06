#include <engine.h>

#include "iscene.h"
#include "utils.h"
#include "iscript.h"

#include "pong/scenes/main_menu_scene.h"
#include "pong/scenes/main_scene.h"

#include <SDL_system.h>
#include <SDL_main.h>

#include <fmt/format.h>

#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <span>
#include <cassert>
#include <memory>
#include <unordered_map>
#include <deque>

int main(int argc, char** argv)
{

	std::string assets_path = "";
	if (argc > 1)
	{
		assets_path = argv[1];
	}
    log(fmt::format("Reading assets from path: {}\n", assets_path));

	engine_application_t app{};
	engine_application_create_desc_t app_cd{};
	app_cd.name = "Pong";
	app_cd.asset_store_path = assets_path.c_str();
    app_cd.width = K_IS_ANDROID ? 0 : 2280;
    app_cd.height = K_IS_ANDROID ? 0 : 1080;
    app_cd.fullscreen = K_IS_ANDROID;


	auto engine_error_code = engineApplicationCreate(&app, app_cd);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineApplicationDestroy(app);
        log(fmt::format("Couldnt create engine application!\n"));
		return -1;
	}

    engine_model_info_t model_info{};
    auto model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "cube.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading CUBE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t cube_geometry{};
    engineApplicationAddGeometryFromMemory(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "cube", &cube_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);

    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "sphere_uv.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading SPHERE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t sphere_geometry{};
    engineApplicationAddGeometryFromMemory(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "sphere", &sphere_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);

    
    engine_font_t font_handle{};
    if (engineApplicationAddFontFromFile(app, "tahoma.ttf", "tahoma_font", &font_handle) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt load font!\n"));
        return -1;
    }

    std::deque<std::unique_ptr<engine::IScene>> scenes;
    scenes.push_back(std::make_unique<pong::MainMenuScene>(app, engine_error_code));
    scenes.push_back(std::make_unique<pong::MainScene>(app, engine_error_code));


    //engine_component_view_t rect_tranform_view{};
    //engineCreateComponentView(&rect_tranform_view);
    //engineSceneComponentViewAttachRectTransformComponent(scene, rect_tranform_view);

    //engine_component_iterator_t begin_it{};
    //engineComponentViewCreateBeginComponentIterator(rect_tranform_view, &begin_it);
    //engine_component_iterator_t end_it{};
    //engineComponentViewCreateEndComponentIterator(rect_tranform_view, &end_it);

    //size_t idx = 0;
    //while (engineComponentIteratorCheckEqual(begin_it, end_it) == false)
    //{
    //    std::cout << idx++ << std::endl;
    //    const auto game_obj = engineComponentIteratorGetGameObject(begin_it);
    //    const auto rect_transform = engineSceneGetRectTransformComponent(scene, game_obj);
    //    std::cout << rect_transform.position[0] << ", " << rect_transform.position[1] << std::endl;
    //    engineComponentIteratorNext(begin_it);
    //}

    //if (begin_it)
    //{
    //    engineDeleteComponentIterator(begin_it);
    //}

    //if (end_it)
    //{
    //    engineDeleteComponentIterator(end_it);
    //}

    //if (rect_tranform_view)
    //{
    //    engineDestroyComponentView(rect_tranform_view);
    //}
    struct fps_counter_t
    {
        float frames_total_time = 0.0f;
        std::uint32_t frames_count = 0;
    };
    fps_counter_t fps_counter{};

	while (true)
	{
		const auto frame_begin = engineApplicationFrameBegine(app);

        if (frame_begin.events & ENGINE_EVENT_QUIT)
        {
            log(fmt::format("Engine requested app quit. Exiting.\n"));
            break;
        }

		if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_ESCAPE))
		{
			log(fmt::format("User pressed ESCAPE key. Exiting.\n"));
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

        for (auto& scene : scenes)
        {
            scene->update(frame_begin.delta_time);
        }

		const auto frame_end = engineApplicationFrameEnd(app);
		if (!frame_end.success)
		{
			log(fmt::format("Frame not finished sucesfully. Exiting.\n"));
			break;
		}
	}

	engineApplicationDestroy(app);

	return 0;
}