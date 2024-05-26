#include "app.h"
#include "iscene.h"
#include "animation_controller.h"
#include <chrono>

#include <fmt/format.h>

namespace
{
inline engine_game_object_t parse_prefab(const project_c::ModelInfo& model_info, engine_scene_t scene)
{
    engine_game_object_t ret = ENGINE_INVALID_GAME_OBJECT_ID;

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
            ret = go;
        }
    }
    assert(ret != ENGINE_INVALID_GAME_OBJECT_ID);

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
    //auto copy_anim_channel_data_vec3 = [](AnimationChannelVec3& out_channel, const engine_animation_channel_data_t& in_channel)
    //    {
    //        //timestamps
    //        out_channel.timestamps.resize(in_channel.timestamps_count);
    //        std::memcpy(out_channel.timestamps.data(), in_channel.timestamps, in_channel.timestamps_count * sizeof(in_channel.timestamps[0]));

    //        // data
    //        out_channel.data.resize(in_channel.data_count / AnimationChannelVec3::DataType::length());
    //        std::memcpy(out_channel.data.data(), in_channel.data, in_channel.data_count * sizeof(in_channel.data[0]));
    //    };

    //auto copy_anim_channel_data_quat = [](AnimationChannelQuat& out_channel, const engine_animation_channel_data_t& in_channel)
    //    {
    //        //timestamps
    //        out_channel.timestamps.resize(in_channel.timestamps_count);
    //        std::memcpy(out_channel.timestamps.data(), in_channel.timestamps, in_channel.timestamps_count * sizeof(in_channel.timestamps[0]));

    //        // data
    //        out_channel.data.resize(in_channel.data_count / AnimationChannelQuat::DataType::length());
    //        std::memcpy(out_channel.data.data(), in_channel.data, in_channel.data_count * sizeof(in_channel.data[0]));
    //    };

    //for (auto anim_idx = 0; anim_idx < model_info.model_info.animations_counts; anim_idx++)
    //{
    //    const auto& anim_in = model_info.model_info.animations_array[anim_idx];
    //    log(fmt::format("Adding animation: {}\n", anim_in.name));
    //    std::map<engine_game_object_t, AnimationChannelData> anim_clip_data;
    //    for (auto channel_idx = 0; channel_idx < anim_in.channels_count; channel_idx++)
    //    {
    //        const auto& in_channel = anim_in.channels[channel_idx];
    //        const auto& go = node_id_to_game_object[in_channel.model_node_index];
    //        assert(anim_clip_data.find(go) == anim_clip_data.end());
    //        AnimationChannelData& out_channel = anim_clip_data[go];
    //        copy_anim_channel_data_vec3(out_channel.translation, in_channel.channel_translation);
    //        copy_anim_channel_data_vec3(out_channel.scale, in_channel.channel_scale);
    //        copy_anim_channel_data_quat(out_channel.rotation, in_channel.channel_rotation);
    //    }
        //script->get_animation_controller().add_animation_clip(anim_in.name, AnimationClip(scene, std::move(anim_clip_data)));
    //}

    return ret;
}

inline engine_application_create_desc_t app_cd()
{
    engine_application_create_desc_t app_cd{};
    app_cd.name = "Project_C";
    app_cd.asset_store_path = "C:\\WORK\\OpenGLPlayground\\assets";
    app_cd.width = K_IS_ANDROID ? 0 : 2280 / 2;
    app_cd.height = K_IS_ANDROID ? 0 : 1080 / 2;
    app_cd.fullscreen = K_IS_ANDROID;
    app_cd.enable_editor = true;
    return app_cd;
}
}  // namespace anonymous

project_c::AppProjectC::AppProjectC()
    : engine::IApplication(app_cd())
{
    const auto load_start = std::chrono::high_resolution_clock::now();

    const std::unordered_map<PrefabType, std::pair<std::string, std::string>> prefabs_data =
    {
        { PREFAB_TYPE_SWORD,        { "weapon-sword.glb", "Textures_mini_arena" }},
        { PREFAB_TYPE_SOLIDER,      { "character-soldier.glb", "Textures_mini_arena" }},
        { PREFAB_TYPE_ORC,          { "character-orc.glb", "Textures_mini_dungeon" }},
        { PREFAB_TYPE_BARREL,       { "barrel.glb", "Textures_mini_dungeon" }},
        { PREFAB_TYPE_FLOOR,        { "floor.glb", "Textures_mini_dungeon" }},
        { PREFAB_TYPE_FLOOR_DETAIL, { "floor-detail.glb", "Textures_mini_dungeon" }},
        { PREFAB_TYPE_WALL,         { "wall.glb", "Textures_mini_dungeon" }},
        { PREFAB_TYPE_CUBE,         { "cube.glb", ""}}
    };

    for (const auto& [type, file_and_basedir] : prefabs_data)
    {
        const auto& [model_file_name, base_dir] = file_and_basedir;
        engine_result_code_t engine_error_code = ENGINE_RESULT_CODE_FAIL;
        prefabs_[type] = std::move(ModelInfo(engine_error_code, get_handle(), model_file_name, base_dir));
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            log(fmt::format("Failed loading prefab: {}\n", type));
        }
    }


    const auto load_end = std::chrono::high_resolution_clock::now();
    const auto ms_load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
    log(fmt::format("Model loading took: {}\n", ms_load_time));
}

engine_game_object_t project_c::AppProjectC::instantiate_prefab(PrefabType type, engine::IScene* scene)
{
    if (type >= PREFAB_TYPE_COUNT)
    {
        log(fmt::format("Invalid prefab type: {}\n", type));
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }

    const auto& prefab = prefabs_[type];
    if (!prefab.is_valid())
    {
        log(fmt::format("Prefab: {} is not valid\n", type));
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    return parse_prefab(prefab, scene->get_handle());
}

void project_c::AppProjectC::run()
{
    struct fps_counter_t
    {
        float frames_total_time = 0.0f;
        std::uint32_t frames_count = 0;
    };
    fps_counter_t fps_counter{};

    while (true)
    {
        const auto frame_begin = engineApplicationFrameBegine(get_handle());

        if (frame_begin.events & ENGINE_EVENT_QUIT)
        {
            log(fmt::format("Engine requested app quit. Exiting.\n"));
            break;
        }

        if (engineApplicationIsKeyboardButtonDown(get_handle(), ENGINE_KEYBOARD_KEY_ESCAPE))
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

        auto scene = get_scene("TestScene");
        auto scene_city = get_scene("CityScene");
        if (engineApplicationIsKeyboardButtonDown(get_handle(), ENGINE_KEYBOARD_KEY_5))
        {
            scene->deactivate();
            scene_city->activate();
        }
        else if (engineApplicationIsKeyboardButtonDown(get_handle(), ENGINE_KEYBOARD_KEY_6))
        {
            scene->activate();
            scene_city->deactivate();
        }
        update_scenes(frame_begin.delta_time);

        const auto frame_end = engineApplicationFrameEnd(get_handle());
        if (!frame_end.success)
        {
            log(fmt::format("Frame not finished sucesfully. Exiting.\n"));
            break;
        }
    }
}