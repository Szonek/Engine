#include <engine.h>
#include <iostream>
#include <format>
#include <vector>
#include <array>

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
	
	engine_scene_t scene{};
	engine_error_code = engineSceneCreate(&scene);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineSceneDestroy(scene);
		engineApplicationDestroy(app);
		std::cout << std::format("Couldnt create scene!\n");
		return -1;
	}

	const float cube_vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
	};
	engine_geometry_t cube_geometry{};
	engineApplicationAddGeometryFromMemory(app, reinterpret_cast<const engine_vertex_attribute_t*>(cube_vertices), 36, nullptr, 0, "cube", &cube_geometry);

	{
		auto camera_go = engineSceneCreateGameObject(scene);
		auto camera_comp = engineSceneAddCameraComponent(scene, camera_go);
		auto camera_transform_comp = engineSceneAddTransformComponent(scene, camera_go);
		camera_comp->enabled = true;
		camera_comp->clip_plane_near = 0.1f;
		camera_comp->clip_plane_far = 100.0f;
		camera_comp->type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
		camera_comp->type_union.perspective_fov = 45.0f;
		camera_transform_comp->position[0] = 0.0f;
		camera_transform_comp->position[1] = 0.0f;
		camera_transform_comp->position[2] = 3.0f;
	}

	const std::vector<std::array<float, 3>> cubes_positions =
	{
		{ 0.0f,  0.0f,  0.0f},
		{ 2.0f,  5.0f, -15.0f},
		{ -1.5f, -2.2f, -2.5f},
		{ -3.8f, -2.0f, -12.3f},
		{ 2.4f, -0.4f, -3.5f},
		{ -1.7f,  3.0f, -7.5f},
		{ 1.3f, -2.0f, -2.5f},
		{ 1.5f,  2.0f, -2.5f},
		{ 1.5f,  0.2f, -1.5f},
		{ -1.3f,  1.0f, -1.5f},
	};

	engine_texture2d_t texture_container{};
	engineApplicationAddTexture2DFromFile(app, "container.jpg", ENGINE_TEXTURE_COLOR_SPACE_LINEAR, "container", &texture_container);

	for (auto i = 0; i < cubes_positions.size(); i++)
	{
		const auto cube = engineSceneCreateGameObject(scene);
		auto mesh_comp = engineSceneAddMeshComponent(scene, cube);
		mesh_comp->geometry = cube_geometry;

		auto tc = engineSceneAddTransformComponent(scene, cube);
		tc->position[0] = cubes_positions[i][0];
		tc->position[1] = cubes_positions[i][1];
		tc->position[2] = cubes_positions[i][2];

		const float angle = 20.0f * i;
		tc->rotation[0] = angle;
		tc->rotation[1] = 0.3f * angle;
		tc->rotation[2] = 0.5f * angle;

		auto material_comp = engineSceneAddMaterialComponent(scene, cube);
		if (i % 2 == 0) // to test default texture
		{
			material_comp->diffuse_texture = texture_container;
		}


		auto nc = engineSceneAddNameComponent(scene, cube);
		const std::string name = std::format("cube_{}", i);
		std::strcpy(nc->name, name.c_str());
	}

	bool run_main_loop = true;
	while (run_main_loop)
	{
		const auto frame_begin = engineApplicationFrameBegine(app);

		engine_error_code = engineApplicationFrameRunScene(app, scene, frame_begin.delta_time);
		if (engine_error_code != ENGINE_RESULT_CODE_OK)
		{
			run_main_loop = false;
			std::cout << std::format("Scene update failed. Exiting main loop\n");
			break;
		}
		const auto frame_end = engineApplicationFrameEnd(app);
		run_main_loop = frame_end.success;
	}

	engineSceneDestroy(scene);
	engineApplicationDestroy(app);
	return 0;
}