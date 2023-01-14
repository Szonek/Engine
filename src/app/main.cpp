#include <engine.h>
#include <iostream>
#include <format>
#include <vector>
#include <array>
#include <cmath>

inline auto get_spherical_coordinates(const auto& cartesian)
{
	const float r = std::sqrt(
		std::pow(cartesian[0], 2) +
		std::pow(cartesian[1], 2) +
		std::pow(cartesian[2], 2)
	);


	float phi = std::atan2(cartesian[2] / cartesian[0], cartesian[0]);
	const float theta = std::acos(cartesian[1] / r);

	if (cartesian[0] < 0)
		phi += 3.1415f;

	std::array<float, 3> ret{ 0.0f };
	ret[0] = r;
	ret[1] = phi;
	ret[2] = theta;
	return ret;
}


inline auto get_cartesian_coordinates(const auto& spherical)
{
	std::array<float, 3> ret{ 0.0f };

	ret[0] = spherical[0] * std::cos(spherical[2]) * std::cos(spherical[1]);
	ret[1] = spherical[0] * std::sin(spherical[2]);
	ret[2] = spherical[0] * std::cos(spherical[2]) * std::sin(spherical[1]);

	return ret;
}

class OribitingCamera
{
public:
	// https://nerdhut.de/2020/05/09/unity-arcball-camera-spherical-coordinates/
	void update(engine_game_object_t entt, engine_application_t app, float dt, engine_scene_t scene)
	{
		if (engineSceneHasCameraComponent(scene, entt))
		{
			auto tc = engineSceneGetTransformComponent(scene, entt);
			auto cc = engineSceneGetCameraComponent(scene, entt);

			if (engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT))
			{
				auto coords = engineApplicationGetMouseCoords(app);

				engine_mouse_coords_t d = coords;
				d.x -= prev_mouse_coords_.x;
				d.y -= prev_mouse_coords_.y;

				if (d.x != 0 || d.y != 0)
				{
					// Rotate the camera left and right
					sc_[1] += d.x * dt * 0.0001f;

					// Rotate the camera up and down
					// Prevent the camera from turning upside down (1.5f = approx. Pi / 2)
					sc_[2] = std::clamp(sc_[2] + d.y * dt * 0.0001f, -1.5f, 1.5f);

					const auto new_position = get_cartesian_coordinates(sc_);
					tc->position[0] = new_position[0] + cc->target[0];
					tc->position[1] = new_position[1] + cc->target[1];
					tc->position[2] = new_position[2] + cc->target[2];

				}


			}

			if (prev_mouse_coords_.x == -1)
			{
				sc_ = get_spherical_coordinates(tc->position);
			}
			prev_mouse_coords_ = engineApplicationGetMouseCoords(app);
		}
	}

private:
	std::array<float, 3> sc_;

	engine_mouse_coords_t prev_mouse_coords_{ -1, -1 };
};


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

	auto camera_go = engineSceneCreateGameObject(scene);
	{
		auto camera_comp = engineSceneAddCameraComponent(scene, camera_go);
		auto camera_transform_comp = engineSceneAddTransformComponent(scene, camera_go);
		camera_comp->enabled = true;
		camera_comp->clip_plane_near = 0.1f;
		camera_comp->clip_plane_far = 100.0f;
		camera_comp->type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
		camera_comp->type_union.perspective_fov = 45.0f;
		camera_transform_comp->position[0] = 0.0f;
		camera_transform_comp->position[1] = 10.0f;
		camera_transform_comp->position[2] = 10.0f;
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

	OribitingCamera oribiting_camera_system;
	while (true)
	{
		const auto frame_begin = engineApplicationFrameBegine(app);

		if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_ESCAPE))
		{
			std::cout << std::format("User pressed ESCAPE key. Exiting.\n");
			break;
		}

		if (engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT))
		{
			const auto mouse_coords = engineApplicationGetMouseCoords(app);
			std::cout << std::format("User pressed LEFT mouse button."
				"Mouse position [x,y]: {},{}\n", mouse_coords.x, mouse_coords.y);
		}

		oribiting_camera_system.update(camera_go, app, frame_begin.delta_time, scene);

		engine_error_code = engineApplicationFrameRunScene(app, scene, frame_begin.delta_time);
		if (engine_error_code != ENGINE_RESULT_CODE_OK)
		{
			std::cout << std::format("Scene update failed. Exiting.\n");
			break;
		}
		const auto frame_end = engineApplicationFrameEnd(app);
		if (!frame_end.success)
		{
			std::cout << std::format("Frame not finished sucesfully. Exiting.\n");

			break;
		}
	}

	engineSceneDestroy(scene);
	engineApplicationDestroy(app);
	return 0;
}