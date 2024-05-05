#include <engine.h>

#include "scene_manager.h"
#include "iscene.h"
#include "utils.h"
#include "iscript.h"

#include "animation_controller.h"
#include "camera_script.h"
#include "scripts.h"
#include "model_info.h"

#include <network/net_client.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

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

namespace project_c
{
class TestScene : public engine::IScene
{
public:
    TestScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
        : IScene(app_handle, scn_mgn, engine_error_code)
    {
        auto camera_script = register_script<CameraScript>();


        if (engineApplicationAddFontFromFile(app_handle, "tahoma.ttf", "tahoma_font") != ENGINE_RESULT_CODE_OK)
        {
            log(fmt::format("Couldnt load font!\n"));
            return;
        }
        std::array<engine_ui_document_data_binding_t, 2> bindings{};
        bindings[0].data_uint32_t = &ui_data_.character_health;
        bindings[0].name = "character_health";
        bindings[0].type = ENGINE_DATA_TYPE_UINT32;

        bindings[1].data_uint32_t = &ui_data_.enemy_health;
        bindings[1].name = "enemy_health";
        bindings[1].type = ENGINE_DATA_TYPE_UINT32;

        engine_error_code = engineApplicationCreateUiDocumentDataHandle(app_handle, "health", bindings.data(), bindings.size(), &ui_data_.handle);

        // load ui doc
        engine_error_code = engineApplicationCreateUiDocumentFromFile(app_handle, "project_c_health_bar.rml", &ui_data_.doc);
        if (ui_data_.doc)
        {
            engineUiDocumentShow(ui_data_.doc);
        }

    }

    void update_hook_begin() override
    {
        engineUiDataHandleDirtyVariable(ui_data_.handle, "character_health");
        engineUiDataHandleDirtyVariable(ui_data_.handle, "enemy_health");
    }

    ~TestScene() 
    {
        engineUiDataHandleDestroy(ui_data_.handle);
        engineApplicationUiDocumentDestroy(ui_data_.doc);
    }
    static constexpr const char* get_name() { return "TestScene"; }

private:
    struct UI_data
    {
        engine_ui_document_t doc;
        engine_ui_data_handle_t handle;
        std::uint32_t character_health = 100;
        std::uint32_t enemy_health = 100;
    };

private:   
    UI_data ui_data_;
};

template<typename TScript>
inline bool parse_model_info_and_create_script(project_c::ModelInfo& model_info, engine::IScene* scene_cpp)
{
    auto scene = scene_cpp->get_handle();
    TScript* script = nullptr;
    std::map<std::uint32_t, engine_game_object_t> node_id_to_game_object;
    for (auto i = 0; i < model_info.model_info.nodes_count; i++)
    {
        const auto& node = model_info.model_info.nodes_array[i];
        node_id_to_game_object[i] = engineSceneCreateGameObject(scene);
        const auto& go = node_id_to_game_object[i];
        if (node.name)
        {
            auto nc = engineSceneAddNameComponent(scene, go);
            std::strncpy(nc.name, node.name, std::size(nc.name));
            engineSceneUpdateNameComponent(scene, go, &nc);
            log(fmt::format("Created entity [id: {}] with name: {}\n", go, nc.name));
        }

        // transform
        {
            auto tc = engineSceneAddTransformComponent(scene, go);
            std::memcpy(&tc.position, node.translate, sizeof(tc.position));
            std::memcpy(&tc.rotation, node.rotation_quaternion, sizeof(tc.rotation));
            std::memcpy(&tc.scale, node.scale, sizeof(tc.scale));
            engineSceneUpdateTransformComponent(scene, go, &tc);
            log(fmt::format("\tAdded transform component\n", go));
        }
        
        if (node.geometry_index != -1)
        {
            auto mc = engineSceneAddMeshComponent(scene, go);
            mc.geometry = model_info.geometries.at(node.geometry_index);
            engineSceneUpdateMeshComponent(scene, go, &mc);
            log(fmt::format("\tAdded mesh component\n", go));
        }

        if (node.material_index != -1)
        {
            auto material_comp = engineSceneAddMaterialComponent(scene, go);
            material_comp.material = model_info.materials.at(node.material_index);
            engineSceneUpdateMaterialComponent(scene, go, &material_comp);
            log(fmt::format("\tAdded material component\n", go));
        }

        if (!node.parent)
        {
            script = scene_cpp->register_script<TScript>(go);
        }
    }

    // hierarchy
    for (auto i = 0; i < model_info.model_info.nodes_count; i++)
    {
        const auto& node = model_info.model_info.nodes_array[i];
        const auto& go = node_id_to_game_object[i];
        if (node.parent)
        {
            // find parent index - not optimal. ToDo: consider having parent index instead parent pointer
            const std::uint32_t parent_index = [&]()
                {
                    for (std::uint32_t j = 0; j < model_info.model_info.nodes_count; j++)
                    {
                        if (&model_info.model_info.nodes_array[j] == node.parent)
                        {
                            return j;
                        }
                    }
                    return std::uint32_t(ENGINE_INVALID_GAME_OBJECT_ID);
                }();


            // add parent component
            auto pc = engineSceneAddParentComponent(scene, go);
            pc.parent = node_id_to_game_object[parent_index];
            engineSceneUpdateParentComponent(scene, go, &pc);
            log(fmt::format("Entity: {} added parent component: {}\n", go, pc.parent));
        }
    }

    // bones
    std::map<uint32_t, std::vector<engine_game_object_t>> skin_to_game_object;
    for (auto skin_idx = 0; skin_idx < model_info.model_info.skins_counts; skin_idx++)
    {
        if (skin_idx == 1)
        {
            break;
        }
        const auto& skin = model_info.model_info.skins_array[skin_idx];
        log(fmt::format("Adding skin: {}\n", skin.name));
        for (auto bone_idx = 0; bone_idx < skin.bones_count; bone_idx++)
        {
            const auto& bone = skin.bones_array[bone_idx];
            const auto& go = node_id_to_game_object[bone.model_node_index];
            skin_to_game_object[skin_idx].push_back(go);

            auto bc = engineSceneAddBoneComponent(scene, go);
            std::memcpy(bc.inverse_bind_matrix, bone.inverse_bind_mat, sizeof(bc.inverse_bind_matrix));
            engineSceneUpdateBoneComponent(scene, go, &bc);
            log(fmt::format("\tAttached entity: {} to the skin.\n", go));
        }
    }

    // update nodes with skin components
    for (auto i = 0; i < model_info.model_info.nodes_count; i++)
    {
        const auto& node = model_info.model_info.nodes_array[i];
        const auto& go = node_id_to_game_object[i];
        auto skin_index = node.skin_index;
        if (skin_index != -1)
        {
            skin_index = 0;
            const auto& bones_game_object_arr = skin_to_game_object[skin_index];
            auto sc = engineSceneAddSkinComponent(scene, go);
            for (auto bone_idx = 0; bone_idx < bones_game_object_arr.size(); bone_idx++)
            {
                sc.bones[bone_idx] = bones_game_object_arr.at(bone_idx);
            }
            engineSceneUpdateSkinComponent(scene, go, &sc);
            log(fmt::format("Entity: {} added skin component for skin index: \n", go, skin_index));
        }
    }

    // animations
    auto copy_anim_channel_data_vec3 = [](AnimationChannelVec3& out_channel, const engine_animation_channel_data_t& in_channel)
        {
            //timestamps
            out_channel.timestamps.resize(in_channel.timestamps_count);
            std::memcpy(out_channel.timestamps.data(), in_channel.timestamps, in_channel.timestamps_count * sizeof(in_channel.timestamps[0]));

            // data
            out_channel.data.resize(in_channel.data_count / AnimationChannelVec3::DataType::length());
            std::memcpy(out_channel.data.data(), in_channel.data, in_channel.data_count * sizeof(in_channel.data[0]));
        };

    auto copy_anim_channel_data_quat = [](AnimationChannelQuat& out_channel, const engine_animation_channel_data_t& in_channel)
        {
            //timestamps
            out_channel.timestamps.resize(in_channel.timestamps_count);
            std::memcpy(out_channel.timestamps.data(), in_channel.timestamps, in_channel.timestamps_count * sizeof(in_channel.timestamps[0]));

            // data
            out_channel.data.resize(in_channel.data_count / AnimationChannelQuat::DataType::length());
            std::memcpy(out_channel.data.data(), in_channel.data, in_channel.data_count * sizeof(in_channel.data[0]));
        };

    for (auto anim_idx = 0; anim_idx < model_info.model_info.animations_counts; anim_idx++)
    {
        const auto& anim_in = model_info.model_info.animations_array[anim_idx];
        log(fmt::format("Adding animation: {}\n", anim_in.name));
        std::map<engine_game_object_t, AnimationChannelData> anim_clip_data;
        for (auto channel_idx = 0; channel_idx < anim_in.channels_count; channel_idx++)
        {
            const auto& in_channel = anim_in.channels[channel_idx];
            const auto& go = node_id_to_game_object[in_channel.model_node_index];
            assert(anim_clip_data.find(go) == anim_clip_data.end());
            AnimationChannelData& out_channel = anim_clip_data[go];
            copy_anim_channel_data_vec3(out_channel.translation, in_channel.channel_translation);
            copy_anim_channel_data_vec3(out_channel.scale, in_channel.channel_scale);
            copy_anim_channel_data_quat(out_channel.rotation, in_channel.channel_rotation);
        }
        script->get_animation_controller().add_animation_clip(anim_in.name, AnimationClip(scene, std::move(anim_clip_data)));
    }

    return true;
}

}  // namespace project_c



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
	app_cd.name = "Project_C";
	app_cd.asset_store_path = assets_path.c_str();
    app_cd.width = K_IS_ANDROID ? 0 : 2280 / 2;
    app_cd.height = K_IS_ANDROID ? 0 : 1080 / 2;
    app_cd.fullscreen = K_IS_ANDROID;
    app_cd.enable_editor = true;

	auto engine_error_code = engineApplicationCreate(&app, app_cd);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineApplicationDestroy(app);
        log(fmt::format("Couldnt create engine application!\n"));
		return -1;
	}

    engine::SceneManager scene_manager(app);
    scene_manager.register_scene<project_c::TestScene>();
    auto scene = scene_manager.get_scene("TestScene");


    const auto load_start = std::chrono::high_resolution_clock::now();

    project_c::ModelInfo model_info_swrd(engine_error_code, app, "weapon-sword.glb", "Textures_mini_arena");
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        return false;
    }

    project_c::ModelInfo model_info_solider(engine_error_code, app, "character-soldier.glb", "Textures_mini_arena");
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        return false;
    }

    project_c::ModelInfo model_info_orc(engine_error_code, app, "character-orc.glb", "Textures_mini_dungeon");
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        return false;
    }
    project_c::ModelInfo model_info_cube(engine_error_code, app, "cube.glb");
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        return false;
    }

    bool load_model = true;

    load_model = project_c::parse_model_info_and_create_script<project_c::Solider>(model_info_solider, scene);
    if (!load_model)
    {
        log(fmt::format("Loading model failed!\n"));
        return -1;
    }

    load_model = project_c::parse_model_info_and_create_script<project_c::Sword>(model_info_swrd, scene);
    if (!load_model)
    {
        log(fmt::format("Loading model failed!\n"));
        return -1;
    }

    //load_model = project_c::parse_model_info_and_create_script<project_c::ControllableEntity>(model_info_cesium, scene);
    //if (!load_model)
    //{
    //    log(fmt::format("Loading model failed!\n"));
    //    return -1;
    //}

    //load_model = project_c::parse_model_info_and_create_script<project_c::Enemy>(model_info_cube, scene);
    load_model = project_c::parse_model_info_and_create_script<project_c::Enemy>(model_info_orc, scene);
    if (!load_model)
    {
        log(fmt::format("Loading model failed!\n"));
        return -1;
    }
    load_model = project_c::parse_model_info_and_create_script<project_c::Floor>(model_info_cube, scene);
    if (!load_model)
    {
        log(fmt::format("Loading model failed!\n"));
        return -1;
    }

    //load_model = project_c::load_solider(app, scene);
    //if (!load_model)
    //{
    //    log(fmt::format("Loading model failed!\n"));
    //    return -1;
    //}

    const auto load_end = std::chrono::high_resolution_clock::now();
    const auto ms_load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
    log(fmt::format("Model loading took: {}\n", ms_load_time));

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