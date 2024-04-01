#include <engine.h>

#include "scene_manager.h"
#include "iscene.h"
#include "utils.h"
#include "iscript.h"
#include <network/net_client.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <SDL_system.h>
#include <SDL_main.h>

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <span>
#include <cassert>
#include <memory>
#include <unordered_map>
#include <map>
#include <deque>
#include <functional>
#include <chrono>

class Camera
{
public:
    Camera(engine_application_t app, engine_scene_t scene)
        : app_(app)
        , scene_(scene)
    {
        go_ = engineSceneCreateGameObject(scene_);
        auto camera_comp = engineSceneAddCameraComponent(scene_, go_);
        camera_comp.enabled = true;
        camera_comp.clip_plane_near = 0.1f;
        camera_comp.clip_plane_far = 1000.0f;
        camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
        camera_comp.type_union.perspective_fov = 45.0f;

        camera_comp.target[0] = 0.0f;
        camera_comp.target[1] = 0.0f;
        camera_comp.target[2] = 0.0f;

        engineSceneUpdateCameraComponent(scene_, go_, &camera_comp);

        auto camera_transform_comp = engineSceneAddTransformComponent(scene_, go_);
        camera_transform_comp.position[0] = 0.0f;
        camera_transform_comp.position[1] = 1.0f;
        camera_transform_comp.position[2] = 5.0f;
        engineSceneUpdateTransformComponent(scene_, go_, &camera_transform_comp);
    }


    void update(float dt)
    {
        const auto mouse_coords = engineApplicationGetMouseCoords(app_);

        const auto dx = mouse_coords.x - mouse_coords_prev_.x;
        const auto dy = mouse_coords.y - mouse_coords_prev_.y;

        if (mouse_coords.x != mouse_coords_prev_.x || mouse_coords.y != mouse_coords_prev_.y)
        {
            mouse_coords_prev_ = mouse_coords;
        }

        constexpr const float rotation_speed = 5.0f;

        if (engineApplicationIsMouseButtonDown(app_, engine_mouse_button_t::ENGINE_MOUSE_BUTTON_LEFT))
        {
            translate({ dx * rotation_speed, dy * rotation_speed, 0.0f });
        }

        if (engineApplicationIsMouseButtonDown(app_, engine_mouse_button_t::ENGINE_MOUSE_BUTTON_RIGHT))
        {
            translate({ 0.0f, 0.0f, dy * rotation_speed });
        }
    }

private:
    inline void translate(const glm::vec3& delta)
    {
        auto tc = engineSceneGetTransformComponent(scene_, go_);
        for (int i = 0; i < std::size(tc.position); i++)
        {
            tc.position[i] += delta[i];
        }
        engineSceneUpdateTransformComponent(scene_, go_, &tc);
    }

private:
    engine_application_t app_ = nullptr;
    engine_scene_t scene_ = nullptr;
    engine_game_object_t go_ = ENGINE_INVALID_GAME_OBJECT_ID;

    engine_coords_2d_t mouse_coords_prev_{};
};

int main(int argc, char** argv)
{
	std::string assets_path = "";
	if (argc > 1)
	{
		assets_path = argv[1];
        log(fmt::format("Reading assets from path: {}\n", assets_path));
	}

    bool run_test_model = false;
    if (argc > 2)
    {
        std::string str = argv[2];
        if (str == "test")
        {
            run_test_model = true;
        }
    }
    log(fmt::format("Running test model: {}\n", run_test_model));

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

    const auto load_start = std::chrono::high_resolution_clock::now();
    engine_model_desc_t model_info{};
    //engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, run_test_model ? "test_skin.gltf" : "test2.glb", &model_info);
    engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "riverdance_dance_free_animation.glb", &model_info);
    //engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "CesiumMan.gltf", &model_info);
    //engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "Stag.gltf", &model_info);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading TABLE model. Exiting!\n");
        return -1;
    }

    std::vector<engine_geometry_t> geometries(model_info.geometries_count);
    if (model_info.geometries_count > 0)
    {
        assert(model_info.geometries_count == 1);
        const auto& geo = model_info.geometries_array[0];
        engine_error_code = engineApplicationAddGeometryFromDesc(app, &geo, "y_bot", &geometries[0]);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed creating geometry for loaded model. Exiting!\n");
            return -1;
        }
    }

    if (model_info.materials_count > 0)
    {
        assert(model_info.materials_count == 1);
        const auto& mat = model_info.materials_array[0];
        if (mat.diffuse_texture_info.data)
        {
            engine_error_code = engineApplicationAddTexture2DFromDesc(app, &mat.diffuse_texture_info, "diffuse", nullptr);
        }
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed creating textured for loaded model. Exiting!\n");
            return -1;
        }
    }

    std::vector<engine_skin_t> skins(model_info.skins_counts);
    if (model_info.skins_counts > 0)
    {
        assert(model_info.skins_counts == 1);
        for (auto i = 0; i < model_info.skins_counts; i++)
        {
            const auto& skin = model_info.skins_array[i];
            engine_error_code = engineApplicationAddSkinFromDesc(app, &skin, "skin", &skins[0]);
            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating textured for loaded model. Exiting!\n");
                return -1;
            }
        }
    }

    if (model_info.animations_counts > 0)
    {
        assert(model_info.animations_counts == 1);       
        for (auto i = 0; i < model_info.animations_counts; i++)
        {
            const auto& anim = model_info.animations_array[i];
            engine_error_code = engineApplicationAddAnimationClipFromDesc(app, &anim, "animation", nullptr);
            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating textured for loaded model. Exiting!\n");
                return -1;
            }
        }
    }

    engine_scene_t new_test_scene{};
    if (engineSceneCreate(&new_test_scene) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt create new scene!\n"));
        return -1;
    }
    Camera camera(app, new_test_scene);

    // add nodes
    std::map<const engine_model_node_desc_t*, engine_game_object_t> model_game_objects{};
    engine_game_object_t go_with_animation = ENGINE_INVALID_GAME_OBJECT_ID;
    if (model_info.nodes_count > 0)
    {
        for (auto i = 0; i < model_info.nodes_count; i++)
        {
            const auto& node = model_info.nodes_array[i];
            const auto go = engineSceneCreateGameObject(new_test_scene);     
            model_game_objects.insert({ &node, go });

            auto nc = engineSceneAddNameComponent(new_test_scene, go);
            if (node.name)
            {
                std::strncpy(nc.name, node.name, std::max(std::strlen(node.name), std::size(nc.name)));
                if (std::strlen(node.name) > std::size(nc.name))
                {
                    log(fmt::format("Couldnt copy full entity name. Orginal name: {}, entity name: {}!\n", nc.name, node.name));
                }
                engineSceneUpdateNameComponent(new_test_scene, go, &nc);
            }
            log(fmt::format("Created entity [id: {}] with name: {}\n", go, nc.name));

            if (node.parent)
            {
                auto pc = engineSceneAddParentComponent(new_test_scene, go);         
                pc.parent = model_game_objects.at(node.parent);
                engineSceneUpdateParentComponent(new_test_scene, go, &pc);
            }

            auto tc = engineSceneAddTransformComponent(new_test_scene, go);
            std::memcpy(tc.position, node.translate, std::size(node.translate) * sizeof(float));
            std::memcpy(tc.scale, node.scale, std::size(node.scale) * sizeof(float));
            std::memcpy(tc.rotation, node.rotation_quaternion, std::size(node.rotation_quaternion) * sizeof(float));
            engineSceneUpdateTransformComponent(new_test_scene, go, &tc);

            if (node.geometry_index != -1 || node.skin_index != -1)
            {
                auto mc = engineSceneAddMeshComponent(new_test_scene, go);
                mc.geometry = node.geometry_index != -1 ? geometries[node.geometry_index] : ENGINE_INVALID_OBJECT_HANDLE;
                mc.skin = node.skin_index != -1 ? skins[node.skin_index] : ENGINE_INVALID_OBJECT_HANDLE;
                engineSceneUpdateMeshComponent(new_test_scene, go, &mc);

                go_with_animation = go;
            }

            // if mesh is present then definitly we need some material to render it
            if (node.geometry_index != -1)
            {
                auto material_comp = engineSceneAddMaterialComponent(new_test_scene, go);
                set_c_array(material_comp.diffuse_color, model_info.materials_array[0].diffuse_color);
                material_comp.diffuse_texture = engineApplicationGetTextured2DByName(app, "diffuse");
                engineSceneUpdateMaterialComponent(new_test_scene, go, &material_comp);

                // animations
                auto anim_comp = engineSceneAddAnimationComponent(new_test_scene, go);
                anim_comp.animations_array[0] = engineApplicationGetAnimationClipByName(app, "animation");
                engineSceneUpdateAnimationComponent(new_test_scene, go, &anim_comp);
            }
        }
    }

    //engineApplicationReleaseModelInfo(app, &model_info);
    const auto load_end = std::chrono::high_resolution_clock::now();
    const auto ms_load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
    log(fmt::format("Model loading took: {}\n", ms_load_time));

    engine_font_t font_handle{};
    if (engineApplicationAddFontFromFile(app, "tahoma.ttf", "tahoma_font", &font_handle) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt load font!\n"));
        return -1;
    }



    std::array<engine_ui_document_data_binding_t, 1> bindings{};
    std::uint32_t health = 100;
    bindings[0].data_uint32_t = &health;
    bindings[0].name = "value";
    bindings[0].type = ENGINE_DATA_TYPE_UINT32;
    engine_ui_data_handle_t ui_data_handle{};
    engine_error_code = engineApplicationCreateUiDocumentDataHandle(app, "health", bindings.data(), bindings.size(), &ui_data_handle);

    // load ui doc
    engine_ui_document_t ui_doc{};
    engine_error_code = engineApplicationCreateUiDocumentFromFile(app, "project_c_health_bar.rml", &ui_doc);
    if (ui_doc)
    {
        engineUiDocumentShow(ui_doc);
    }


    //engine::SceneManager scene_manager(app);
    //scene_manager.register_scene<project_c::Overworld>();

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
        engineUiDataHandleDirtyVariable(ui_data_handle, "value");

        //scene_manager.update(frame_begin.delta_time);
       
        camera.update(frame_begin.delta_time);

        if (engineSceneHasAnimationComponent(new_test_scene, go_with_animation))
        {
            auto anim_comp = engineSceneGetAnimationComponent(new_test_scene, go_with_animation);
            if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_F) && anim_comp.animations_state[0] == ENGINE_ANIMATION_CLIP_STATE_NOT_PLAYING)
            {
                anim_comp.animations_state[0] = ENGINE_ANIMATION_CLIP_STATE_PLAYING;
                engineSceneUpdateAnimationComponent(new_test_scene, go_with_animation, &anim_comp);
            }
        }

        engineApplicationFrameSceneUpdatePhysics(app, new_test_scene, frame_begin.delta_time);
        engineApplicationFrameSceneUpdateGraphics(app, new_test_scene, frame_begin.delta_time);

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