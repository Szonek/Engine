#include <engine.h>

#include "scene_manager.h"
#include "iscene.h"
#include "utils.h"
#include "iscript.h"

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

namespace project_c
{
class CameraScript : public engine::IScript
{
public:
    CameraScript(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto camera_comp = engineSceneAddCameraComponent(scene, go_);
        camera_comp.enabled = true;
        camera_comp.clip_plane_near = 0.1f;
        camera_comp.clip_plane_far = 100.0f;
        camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
        camera_comp.type_union.perspective_fov = 45.0f;
        engineSceneUpdateCameraComponent(scene, go_, &camera_comp);

        auto camera_transform_comp = engineSceneAddTransformComponent(scene, go_);
        camera_transform_comp.position[1] = 5.0f;
        camera_transform_comp.position[2] = 3.0f;
        engineSceneUpdateTransformComponent(scene, go_, &camera_transform_comp);
    }
};

class FloorScript : public engine::IScript
{
public:
    FloorScript(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for floor script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.scale[0] = 5.0f;
        tc.scale[1] = 0.2f;
        tc.scale[2] = 5.0f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto bc = engineSceneAddColliderComponent(scene, go_);
        bc.type = ENGINE_COLLIDER_TYPE_BOX;
        bc.bounciness = 0.5f;
        engineSceneUpdateColliderComponent(scene, go_, &bc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.58f, 0.259f, 0.325f, 0.0f });
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    }
};

class TableScript : public engine::IScript
{
public:
    TableScript(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        //mesh_comp.geometry = engineApplicationGetGeometryByName(app, "table");
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for table script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = 0.0f;
        tc.position[1] = 0.2f;
        tc.position[2] = 0.0f;

        tc.scale[0] = 2.0f;
        tc.scale[1] = 0.4f;
        tc.scale[2] = 1.0f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto bc = engineSceneAddColliderComponent(scene, go_);
        bc.type = ENGINE_COLLIDER_TYPE_BOX;
        bc.bounciness = 0.3f;
        engineSceneUpdateColliderComponent(scene, go_, &bc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.7f, 0.5f, 0.1f, 0.0f });
        //material_comp.diffuse_texture = 1;
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    }
};

class TestObstacle : public engine::IScript
{
public:
    TestObstacle(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        //mesh_comp.geometry = engineApplicationGetGeometryByName(app, "table");
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for table script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = 1.0f;
        tc.position[1] = 0.2f;
        tc.position[2] = 0.0f;

        tc.scale[0] = 0.1f;
        tc.scale[1] = 0.4f;
        tc.scale[2] = 0.1f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto rb = engineSceneAddRigidBodyComponent(scene, go_);
        rb.mass = 1.0f;
        engineSceneUpdateRigidBodyComponent(scene, go_, &rb);

        auto bc = engineSceneAddColliderComponent(scene, go_);
        bc.type = ENGINE_COLLIDER_TYPE_BOX;
        //bc.bounciness = 0.3f;
        engineSceneUpdateColliderComponent(scene, go_, &bc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.1f, 0.1f, 0.1f, 0.0f });
        //material_comp.diffuse_texture = 1;
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    }
};

class CatScript : public  engine::IScript
{
public:
    CatScript(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "sphere");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for cat script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = 0.0f;
        tc.position[1] = 2.0f;
        tc.position[2] = 0.0f;

        tc.scale[0] = 0.2f;
        tc.scale[1] = 0.2f;
        tc.scale[2] = 0.2f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto rb = engineSceneAddRigidBodyComponent(scene, go_);
        rb.mass = 5.0f;
        engineSceneUpdateRigidBodyComponent(scene, go_, &rb);

        auto bc = engineSceneAddColliderComponent(scene, go_);
        bc.type = ENGINE_COLLIDER_TYPE_SPHERE;
        bc.collider.sphere.radius = 1.0f;
        bc.bounciness = 0.7f;
        engineSceneUpdateColliderComponent(scene, go_, &bc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 0.0f });
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    }

    void update(float dt) override
    {
        auto app = my_scene_->get_app_handle();
        auto scene = my_scene_->get_handle();
        bool rb_needs_update = false;
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_RIGHT))
        {
            auto rb = engineSceneGetRigidBodyComponent(scene, go_);
            rb.linear_velocity[0] += 0.01f * dt;
            engineSceneUpdateRigidBodyComponent(scene, go_, &rb);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_LEFT))
        {
            auto rb = engineSceneGetRigidBodyComponent(scene, go_);
            rb.linear_velocity[0] -= 0.01f * dt;
            engineSceneUpdateRigidBodyComponent(scene, go_, &rb);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_UP))
        {
            auto rb = engineSceneGetRigidBodyComponent(scene, go_);
            rb.linear_velocity[2] -= 0.01f * dt;
            engineSceneUpdateRigidBodyComponent(scene, go_, &rb);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_DOWN))
        {
            auto rb = engineSceneGetRigidBodyComponent(scene, go_);
            rb.linear_velocity[2] += 0.01f * dt;
            engineSceneUpdateRigidBodyComponent(scene, go_, &rb);
        }

        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_SPACE))
        {
            auto rb = engineSceneGetRigidBodyComponent(scene, go_);
            rb.linear_velocity[1] += 0.05f * dt;
            rb.linear_velocity[1] = std::min(rb.linear_velocity[1], 3.0f);
            engineSceneUpdateRigidBodyComponent(scene, go_, &rb);
        }

    }
};


class TestScene_0 : public engine::IScene
{
public:
    TestScene_0(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
        : IScene(app_handle, scn_mgn, engine_error_code)
    {
        const float gravity_vec[3] = { 0.0f, -10.0f, 0.0f };
        engineSceneSetGravityVector(scene_, gravity_vec);
        register_script<CameraScript>();
        register_script<FloorScript>();
        register_script<TableScript>();
        register_script<CatScript>();
        register_script<TestObstacle>();
    }

    ~TestScene_0() = default;
    static constexpr const char* get_name() { return "TestScene_0"; }

};
}


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
    app_cd.width = K_IS_ANDROID ? 0 : 2280 / 2;
    app_cd.height = K_IS_ANDROID ? 0 : 1080 / 2;
    app_cd.fullscreen = K_IS_ANDROID;


	auto engine_error_code = engineApplicationCreate(&app, app_cd);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineApplicationDestroy(app);
        log(fmt::format("Couldnt create engine application!\n"));
		return -1;
	}

    // cube
    engine_model_info_t model_info{};
    engine_result_code_t model_info_result = ENGINE_RESULT_CODE_FAIL;

    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "cube.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading CUBE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t cube_geometry{};
    engineApplicationAddGeometryFromMemory(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "cube", &cube_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);

    // sphere
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

    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "table.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading TABLE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t table_geometry{};
    engineApplicationAddGeometryFromMemory(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "table", &table_geometry);


    engine_texture2d_t table_diffuse_texture{};
    if (engineApplicationAddTexture2DFromMemory(app, &model_info.materials_array[0].diffuse_texture_info, "table_diffuse_texture", &table_diffuse_texture) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt create diffuse texture for geometry!!\n"));
        return -1;
    }
    engineApplicationReleaseModelInfo(app, &model_info);

    engine_font_t font_handle{};
    if (engineApplicationAddFontFromFile(app, "tahoma.ttf", "tahoma_font", &font_handle) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt load font!\n"));
        return -1;
    }

    engine::SceneManager scene_manager(app);
    scene_manager.register_scene<project_c::TestScene_0>();

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

        scene_manager.update(frame_begin.delta_time);

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