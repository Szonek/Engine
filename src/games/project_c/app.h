#pragma once

#include "iapplication.h"
#include "scene_manager.h"
#include "iscene.h"
#include "animation_controller.h"

#include "model_info.h"

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <array>


namespace project_c
{
enum PrefabType
{
    PREFAB_TYPE_SOLIDER,
    PREFAB_TYPE_SWORD,
    PREFAB_TYPE_ORC,
    PREFAB_TYPE_BARREL,
    PREFAB_TYPE_FLOOR,
    PREFAB_TYPE_FLOOR_DETAIL,
    PREFAB_TYPE_WALL,
    PREFAB_TYPE_CUBE,
    PREFAB_TYPE_COUNT
};

class AppProjectC : public engine::IApplication
{
public:
    AppProjectC();

    template<typename Tscript, typename... Targs>
    Tscript* instantiate_prefab(PrefabType type, engine::IScene* scene, Targs... targs)
    {
        if (type >= PREFAB_TYPE_COUNT)
        {
            log(fmt::format("Invalid prefab type: {}\n", type));
            return nullptr;
        }

        const auto& prefab = prefabs_[type];
        if (!prefab.is_valid())
        {
            log(fmt::format("Prefab: {} is not valid\n", type));
            return nullptr;
        }

        return parse_prefab_and_create_script<Tscript>(prefab, scene, targs...);
    }

    void run();

private:
    template<typename TScript, typename... Targs>
    inline TScript* parse_prefab_and_create_script(const project_c::ModelInfo& model_info, engine::IScene* scene_cpp, Targs... targs)
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
                script = scene_cpp->register_script<TScript>(go, targs...);
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

        return script;
    }

private:
    std::array<ModelInfo, PREFAB_TYPE_COUNT> prefabs_;
};
} // namespace project_c