#include <engine.h>

#include "utils.h"
#include "iscript.h"

#include "pong/global_constants.h"
#include "pong/ball_script.h"
#include "pong/player_paddle_script.h"
#include "pong/goal_net_script.h"
#include "pong/wall_script.h"
#include "pong/camera_script.h"

#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <span>
#include <cassert>
#include <memory>
#include <unordered_map>

#include <SDL_system.h>
#include <SDL_main.h>

#include <fmt/format.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>


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

	
	engine_scene_t scene{};
	engine_error_code = engineSceneCreate(&scene);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineSceneDestroy(scene);
		engineApplicationDestroy(app);
        log(fmt::format("Couldnt create scene!\n"));
		return -1;
	}
    const float gravity[3] = { 0.0f, 0.0f, 0.0f };
    engineSceneSetGravityVector(scene, gravity);

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

    struct fps_counter_t
    {
        float frames_total_time = 0.0f;
        std::uint32_t frames_count = 0;
    };
    fps_counter_t fps_counter{};


    CameraScript camera_script(app, scene);
    BallScript ball_script(app, scene);
    LeftPlayerPaddleScript right_player_script(app, scene);
    RightPlayerPaddleScript left_player_script(app, scene);

    right_player_script.ball_script_ = &ball_script;
    left_player_script.ball_script_ = &ball_script;

    LeftGoalNetScript left_goal_net_script(app, scene);
    RightGoalNetScript right_goal_net_script(app, scene);
    left_goal_net_script.ball_script_ = &ball_script;
    left_goal_net_script.player_paddel_script_ = &left_player_script;
    right_goal_net_script.ball_script_ = &ball_script;
    right_goal_net_script.player_paddel_script_ = &right_player_script;

    WallTopScript top_wall(app, scene);
    BottomTopScript bottom_wall(app, scene);

    std::unordered_map<engine_game_object_t, IScript*> scene_manager;
    scene_manager.reserve(1024);
    scene_manager[camera_script.get_game_object()] = &camera_script;
    scene_manager[ball_script.get_game_object()] = &ball_script;
    scene_manager[right_player_script.get_game_object()] = &right_player_script;
    scene_manager[left_player_script.get_game_object()] = &left_player_script;
    scene_manager[left_goal_net_script.get_game_object()] = &left_goal_net_script;
    scene_manager[right_goal_net_script.get_game_object()] = &right_goal_net_script;
    scene_manager[top_wall.get_game_object()] = &top_wall;
    scene_manager[bottom_wall.get_game_object()] = &bottom_wall;


    engine_component_view_t rect_tranform_view{};
    engineCreateComponentView(&rect_tranform_view);
    engineSceneComponentViewAttachRectTransformComponent(scene, rect_tranform_view);

    engine_component_iterator_t begin_it{};
    engineComponentViewCreateBeginComponentIterator(rect_tranform_view, &begin_it);
    engine_component_iterator_t end_it{};
    engineComponentViewCreateEndComponentIterator(rect_tranform_view, &end_it);

    size_t idx = 0;
    while (engineComponentIteratorCheckEqual(begin_it, end_it) == false)
    {
        std::cout << idx++ << std::endl;
        const auto game_obj = engineComponentIteratorGetGameObject(begin_it);
        const auto rect_transform = engineSceneGetRectTransformComponent(scene, game_obj);
        std::cout << rect_transform.position[0] << ", " << rect_transform.position[1] << std::endl;
        engineComponentIteratorNext(begin_it);
    }

    if (begin_it)
    {
        engineDeleteComponentIterator(begin_it);
    }

    if (end_it)
    {
        engineDeleteComponentIterator(end_it);
    }

    if (rect_tranform_view)
    {
        engineDestroyComponentView(rect_tranform_view);
    }

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

		if (engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT))
		{
			const auto mouse_coords = engineApplicationGetMouseCoords(app);
			log(fmt::format("User pressed LEFT mouse button."
				"Mouse position [x,y]: {},{}\n", mouse_coords.x, mouse_coords.y));
		}

        const float dt = frame_begin.delta_time;
        //const float dt = 50.0f;
        fps_counter.frames_count += 1;
        fps_counter.frames_total_time += dt;



        if (fps_counter.frames_total_time > 1000.0f)
        {
            log(fmt::format("FPS: {}, latency: {} ms. \n", 
                fps_counter.frames_count, fps_counter.frames_total_time / fps_counter.frames_count));
            fps_counter = {};

            const auto log_score = fmt::format("Left: {}, Right: {}\n", left_player_script.get_score(), right_player_script.get_score());
            engineLog(log_score.c_str());
        }

		engine_error_code = engineApplicationFrameSceneUpdatePhysics(app, scene, frame_begin.delta_time);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            log(fmt::format("Scene physcis update failed. Exiting.\n"));
            break;
        }



        std::size_t num_collisions = 0;
        const engine_collision_info_t* collisions_list = nullptr;
        engineSceneGetCollisions(scene, &num_collisions, &collisions_list);
        for(std::size_t i = 0; i < num_collisions; i++)
        {
            const auto& col = collisions_list[i];
            IScript::collision_t collision{};
            collision.contact_points.resize(col.contact_points_count);
            for (std::size_t j = 0; j < col.contact_points_count; j++)
            {
                collision.contact_points[j].lifetime = col.contact_points[j].lifetime;
                collision.contact_points[j].point[0] = col.contact_points[j].point_object_a[0];
                collision.contact_points[j].point[1] = col.contact_points[j].point_object_a[1];
                collision.contact_points[j].point[2] = col.contact_points[j].point_object_a[2];
            }

            collision.other = col.object_b;
            scene_manager[col.object_a]->on_collision(collision);

            collision.other = col.object_a;
            scene_manager[col.object_b]->on_collision(collision);

            //log(fmt::format("Num collisions: {}, num cp: {} \n", num_collisions, collisions_list[0].contact_points_count));
            //log(fmt::format("Pt_a: {}, {}, {}\n", collisions_list[0].contact_points->point_object_a[0], collisions_list[0].contact_points->point_object_a[1], collisions_list[0].contact_points->point_object_a[2]));
            //log(fmt::format("Pt_b: {}, {}, {}\n", collisions_list[0].contact_points->point_object_b[0], collisions_list[0].contact_points->point_object_b[1], collisions_list[0].contact_points->point_object_b[2]));
        }

        for (auto& [go, script] : scene_manager)
        {
            script->update(dt);
        }

		engine_error_code = engineApplicationFrameSceneUpdateGraphics(app, scene, dt);
		if (engine_error_code != ENGINE_RESULT_CODE_OK)
		{
			log(fmt::format("Scene update failed. Exiting.\n"));
			break;
		}
		const auto frame_end = engineApplicationFrameEnd(app);
		if (!frame_end.success)
		{
			log(fmt::format("Frame not finished sucesfully. Exiting.\n"));
			break;
		}
	}

	engineSceneDestroy(scene);
	engineApplicationDestroy(app);

	return 0;
}