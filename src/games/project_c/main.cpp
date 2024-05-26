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
    //std::string assets_path = "";
    //if (argc > 1)
    //{
    //    assets_path = argv[1];
    //    log(fmt::format("Reading assets from path: {}\n", assets_path));
    //}

    //bool run_test_model = false;
    //if (argc > 2)
    //{
    //    std::string str = argv[2];
    //    if (str == "test")
    //    {
    //        run_test_model = true;
    //    }
    //}
    //log(fmt::format("Running test model: {}\n", run_test_model));

	//engine_application_t app{};
	//engine_application_create_desc_t app_cd{};
	//app_cd.name = "Project_C";
	//app_cd.asset_store_path = assets_path.c_str();
 //   app_cd.width = K_IS_ANDROID ? 0 : 2280 / 2;
 //   app_cd.height = K_IS_ANDROID ? 0 : 1080 / 2;
 //   app_cd.fullscreen = K_IS_ANDROID;
 //   app_cd.enable_editor = true;

	//auto engine_error_code = engineApplicationCreate(&app, app_cd);
	//if (engine_error_code != ENGINE_RESULT_CODE_OK)
	//{
	//	engineApplicationDestroy(app);
 //       log(fmt::format("Couldnt create engine application!\n"));
	//	return -1;
	//}

    //project_c::ModelInfo model_info_swrd(engine_error_code, app, "weapon-sword.glb", "Textures_mini_arena");
    //if (engine_error_code != ENGINE_RESULT_CODE_OK)
    //{
    //    return false;
    //}

    //project_c::ModelInfo model_info_solider(engine_error_code, app, "character-soldier.glb", "Textures_mini_arena");
    //if (engine_error_code != ENGINE_RESULT_CODE_OK)
    //{
    //    return false;
    //}

    //project_c::ModelInfo model_info_orc(engine_error_code, app, "character-orc.glb", "Textures_mini_dungeon");
    //if (engine_error_code != ENGINE_RESULT_CODE_OK)
    //{
    //    return false;
    //}
    //project_c::ModelInfo model_info_barrel(engine_error_code, app, "barrel.glb", "Textures_mini_dungeon");
    //if (engine_error_code != ENGINE_RESULT_CODE_OK)
    //{
    //    return false;
    //}
    //project_c::ModelInfo model_info_floor(engine_error_code, app, "floor.glb", "Textures_mini_dungeon");
    //if (engine_error_code != ENGINE_RESULT_CODE_OK)
    //{
    //    return false;
    //}

    //project_c::ModelInfo model_info_floor_detail(engine_error_code, app, "floor-detail.glb", "Textures_mini_dungeon");
    //if (engine_error_code != ENGINE_RESULT_CODE_OK)
    //{
    //    return false;
    //}


    //project_c::ModelInfo model_info_wall(engine_error_code, app, "wall.glb", "Textures_mini_dungeon");
    //if (engine_error_code != ENGINE_RESULT_CODE_OK)
    //{
    //    return false;
    //}


    //project_c::ModelInfo model_info_cube(engine_error_code, app, "cube.glb");
    //if (engine_error_code != ENGINE_RESULT_CODE_OK)
    //{
    //    return false;
    //}
    //engine_material_create_desc_t cube_material_desc = engineApplicationInitMaterialDesc(app);
    //cube_material_desc.shader_type = ENGINE_SHADER_TYPE_UNLIT;
    //set_c_array(cube_material_desc.diffuse_color, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
    //engineApplicationAddMaterialFromDesc(app, &cube_material_desc, "light_material", nullptr);


    //bool load_model = true;

    //load_model = project_c::parse_model_info_and_create_script<project_c::Solider>(model_info_solider, scene);
    //load_model = project_c::parse_model_info_and_create_script<project_c::Solider>(model_info_solider, scene_city);
    //if (!load_model)
    //{
    //    log(fmt::format("Loading model failed!\n"));
    //    return -1;
    //}

    //load_model = project_c::parse_model_info_and_create_script<project_c::Sword>(model_info_swrd, scene);
    //if (!load_model)
    //{
    //    log(fmt::format("Loading model failed!\n"));
    //    return -1;
    //}

    //load_model = project_c::parse_model_info_and_create_script<project_c::Sword>(model_info_swrd, scene);
    //if (!load_model)
    //{
    //    log(fmt::format("Loading model failed!\n"));
    //    return -1;
    //}

    //project_c::parse_model_info_and_create_script<project_c::Enemy>(model_info_orc, scene, 0.0f, -1.0f);
    //project_c::parse_model_info_and_create_script<project_c::Enemy>(model_info_orc, scene, 0.0f, 0.0f);
    //project_c::parse_model_info_and_create_script<project_c::Enemy>(model_info_orc, scene, 0.0f, 1.0f);

    //project_c::parse_model_info_and_create_script<project_c::Enemy>(model_info_orc, scene, 2.5f, -1.0f);
    //project_c::parse_model_info_and_create_script<project_c::Enemy>(model_info_orc, scene, 2.5f, 0.0f);
    //project_c::parse_model_info_and_create_script<project_c::Enemy>(model_info_orc, scene, 2.5f, 1.0f);

    //// light
    //scene->register_script<project_c::MainLight>();
    //scene_city->register_script<project_c::MainLight>();
    //scene->register_script<project_c::PointLight>();
    //scene->register_script<project_c::SpotLight>();
   
    //auto generate_floor = [&model_info_wall, &model_info_floor_detail, &model_info_floor](std::int32_t map_border_distance_x, std::int32_t map_border_distance_z, auto& scene)
    //{
    //    std::mt19937 rng(42);
    //    std::uniform_int_distribution<std::mt19937::result_type> dist6(0, 1);
    //    for (std::int32_t x = -map_border_distance_x; x <= map_border_distance_x; x++)
    //    {
    //        for (std::int32_t z = -map_border_distance_z; z <= map_border_distance_z; z++)
    //        {
    //            if (x == -map_border_distance_x || x == map_border_distance_x || z == -map_border_distance_z || z == map_border_distance_z)
    //            {
    //                project_c::parse_model_info_and_create_script<project_c::Wall>(model_info_wall, scene, x, z);
    //            }
    //            else
    //            {
    //                project_c::ModelInfo* model_info = nullptr;
    //                if (dist6(rng))
    //                {
    //                    model_info = &model_info_floor_detail;
    //                }
    //                else
    //                {
    //                    model_info = &model_info_floor;
    //                }
    //                project_c::parse_model_info_and_create_script<project_c::Floor>(*model_info, scene, x, z);
    //            }

    //        }
    //    }
    //};

    //generate_floor(9, 3, scene);
    //generate_floor(3, 3, scene_city);

    //if (!load_model)
    //{
    //    log(fmt::format("Loading model failed!\n"));
    //    return -1;
    //}
    //load_model = project_c::parse_model_info_and_create_script<project_c::Barrel>(model_info_barrel, scene);
    //if (!load_model)
    //{
    //    log(fmt::format("Loading model failed!\n"));
    //    return -1;
    //}
    //load_model = project_c::load_solider(app, scene);
    //if (!load_model)
    //{
    //    log(fmt::format("Loading model failed!\n"));
    //    return -1;
    //}


 //   struct fps_counter_t
 //   {
 //       float frames_total_time = 0.0f;
 //       std::uint32_t frames_count = 0;
 //   };
 //   fps_counter_t fps_counter{};

	//while (true)
	//{
	//	const auto frame_begin = engineApplicationFrameBegine(app);

 //       if (frame_begin.events & ENGINE_EVENT_QUIT)
 //       {
 //           log(fmt::format("Engine requested app quit. Exiting.\n"));
 //           break;
 //       }

	//	if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_ESCAPE))
	//	{
	//		log(fmt::format("User pressed ESCAPE key. Exiting.\n"));
	//		break;
	//	}

 //       static bool button_1_frames[2] = { false, false };
 //       button_1_frames[fps_counter.frames_count % 2] = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_5);
 //       if (button_1_frames[0] && !button_1_frames[1])
 //       {
 //           if (scene->is_active())
 //           {
 //               scene->deactivate();
 //               scene_city->activate();
 //           }
 //           else
 //           {
 //               scene->activate();
 //               scene_city->deactivate();
 //           };
 //       }

 //       fps_counter.frames_count += 1;
 //       fps_counter.frames_total_time += frame_begin.delta_time;
 //       if (fps_counter.frames_total_time > 1000.0f)
 //       {
 //           log(fmt::format("FPS: {}, latency: {} ms. \n",
 //               fps_counter.frames_count, fps_counter.frames_total_time / fps_counter.frames_count));
 //           fps_counter = {};
 //       }

 //       scene_manager.update(frame_begin.delta_time);

	//	const auto frame_end = engineApplicationFrameEnd(app);
	//	if (!frame_end.success)
	//	{
	//		log(fmt::format("Frame not finished sucesfully. Exiting.\n"));
	//		break;
	//	}
	//}
    //engineApplicationDestroy(app);
	return 0;
}