#include "prefab.h"

#include <fmt/format.h>
#include <iscene.h>

project_c::Prefab::Prefab(Prefab&& rhs) noexcept
{
    std::swap(app_, rhs.app_);
    std::swap(model_info_, rhs.model_info_);
    std::swap(geometries_, rhs.geometries_);
    std::swap(textures_, rhs.textures_);
    std::swap(materials_, rhs.materials_);
}

project_c::Prefab& project_c::Prefab::operator=(Prefab&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::swap(app_, rhs.app_);
        std::swap(model_info_, rhs.model_info_);
        std::swap(geometries_, rhs.geometries_);
        std::swap(textures_, rhs.textures_);
        std::swap(materials_, rhs.materials_);
    }
    return *this;
}

project_c::Prefab::~Prefab()
{
    if (is_valid())
    {
        for (const auto& g : geometries_)
        {
            engineApplicationDestroyGeometry(app_, g);
        }
        for (const auto& t : textures_)
        {
            engineApplicationDestroyTexture2D(app_, t);
        }
        for (const auto& m : materials_)
        {
            engineApplicationDestroyMaterial(app_, m);
        }
        engineApplicationReleaseModelDesc(app_, &model_info_);
    }
}

project_c::Prefab::Prefab(engine_result_code_t& engine_error_code, engine_application_t& app, std::string_view model_file_name, std::string_view base_dir)
    : app_(app)
{

    engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, model_file_name.data(), base_dir.data(), &model_info_);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading TABLE model. Exiting!\n");
        return;
    }

    geometries_ = std::vector(model_info_.geometries_count, ENGINE_INVALID_OBJECT_HANDLE);
    for (std::uint32_t i = 0; i < model_info_.geometries_count; i++)
    {
        const auto& geo = model_info_.geometries_array[i];
        engine_error_code = engineApplicationCreateGeometryFromDesc(app, &geo, model_file_name.data(), &geometries_[i]);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed creating geometry for loaded model. Exiting!\n");
            return;
        }
    }

    textures_ = std::vector<engine_texture2d_t>(model_info_.textures_count, ENGINE_INVALID_OBJECT_HANDLE);
    for (std::uint32_t i = 0; i < model_info_.textures_count; i++)
    {
        const auto name = "unnamed_texture_" + std::to_string(i);
        engine_error_code = engineApplicationCreateTexture2DFromDesc(app, &model_info_.textures_array[i], name.c_str(), &textures_[i]);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed creating texture for loaded model. Exiting!\n");
            return;
        }
    }

    materials_ = std::vector<engine_material_t>(model_info_.materials_count, ENGINE_INVALID_OBJECT_HANDLE);
    for (std::uint32_t i = 0; i < model_info_.materials_count; i++)
    {
        const auto& mat = model_info_.materials_array[i];
        engine_material_create_desc_t mat_create_desc = engineApplicationInitMaterialDesc(app);
        mat_create_desc.shader_type = ENGINE_SHADER_TYPE_LIT;
        set_c_array(mat_create_desc.diffuse_color, mat.diffuse_color);
        if (mat.diffuse_texture_index != -1)
        {
            mat_create_desc.diffuse_texture = textures_.at(mat.diffuse_texture_index);
        }
        engine_error_code = engineApplicationCreateMaterialFromDesc(app, &mat_create_desc, mat.name, &materials_[i]);

        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed creating textured for loaded model. Exiting!\n");
            return;
        }
    }
}

project_c::PrefabResult project_c::Prefab::instantiate(engine::IScene* scene_cpp) const
{
    auto scene = scene_cpp->get_handle();
    project_c::PrefabResult ret{};
    ret.go = ENGINE_INVALID_GAME_OBJECT_ID;

    std::map<std::uint32_t, engine_game_object_t> node_id_to_game_object;
    for (auto i = 0; i < model_info_.nodes_count; i++)
    {
        const auto& node = model_info_.nodes_array[i];
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
            mc.geometry = geometries_.at(node.geometry_index);
            engineSceneUpdateMeshComponent(scene, go, &mc);
            log(fmt::format("\tAdded mesh component\n", go));
        }

        if (node.material_index != -1)
        {
            auto material_comp = engineSceneAddMaterialComponent(scene, go);
            material_comp.material = materials_.at(node.material_index);
            engineSceneUpdateMaterialComponent(scene, go, &material_comp);
            log(fmt::format("\tAdded material component\n", go));
        }

        if (!node.parent)
        {
            ret.go = go;
        }
    }
    assert(ret.go != ENGINE_INVALID_GAME_OBJECT_ID);

    // hierarchy
    for (auto i = 0; i < model_info_.nodes_count; i++)
    {
        const auto& node = model_info_.nodes_array[i];
        const auto& go = node_id_to_game_object[i];
        if (node.parent)
        {
            // find parent index - not optimal. ToDo: consider having parent index instead parent pointer
            const std::uint32_t parent_index = [&]()
                {
                    for (std::uint32_t j = 0; j < model_info_.nodes_count; j++)
                    {
                        if (&model_info_.nodes_array[j] == node.parent)
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
    for (auto skin_idx = 0; skin_idx < model_info_.skins_counts; skin_idx++)
    {
        if (skin_idx == 1)
        {
            break;
        }
        const auto& skin = model_info_.skins_array[skin_idx];
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
    for (auto i = 0; i < model_info_.nodes_count; i++)
    {
        const auto& node = model_info_.nodes_array[i];
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
    auto copy_anim_channel_data_vec3 = [](project_c::AnimationChannelVec3& out_channel, const engine_animation_channel_data_t& in_channel)
        {
            //timestamps
            out_channel.timestamps.resize(in_channel.timestamps_count);
            std::memcpy(out_channel.timestamps.data(), in_channel.timestamps, in_channel.timestamps_count * sizeof(in_channel.timestamps[0]));

            // data
            out_channel.data.resize(in_channel.data_count / project_c::AnimationChannelVec3::DataType::length());
            std::memcpy(out_channel.data.data(), in_channel.data, in_channel.data_count * sizeof(in_channel.data[0]));
        };

    auto copy_anim_channel_data_quat = [](project_c::AnimationChannelQuat& out_channel, const engine_animation_channel_data_t& in_channel)
        {
            //timestamps
            out_channel.timestamps.resize(in_channel.timestamps_count);
            std::memcpy(out_channel.timestamps.data(), in_channel.timestamps, in_channel.timestamps_count * sizeof(in_channel.timestamps[0]));

            // data
            out_channel.data.resize(in_channel.data_count / project_c::AnimationChannelQuat::DataType::length());
            std::memcpy(out_channel.data.data(), in_channel.data, in_channel.data_count * sizeof(in_channel.data[0]));
        };

    for (auto anim_idx = 0; anim_idx < model_info_.animations_counts; anim_idx++)
    {
        const auto& anim_in = model_info_.animations_array[anim_idx];
        log(fmt::format("Adding animation: {}\n", anim_in.name));
        std::map<engine_game_object_t, project_c::AnimationChannelData> anim_clip_data;
        for (auto channel_idx = 0; channel_idx < anim_in.channels_count; channel_idx++)
        {
            const auto& in_channel = anim_in.channels[channel_idx];
            const auto& go = node_id_to_game_object[in_channel.model_node_index];
            assert(anim_clip_data.find(go) == anim_clip_data.end());
            project_c::AnimationChannelData& out_channel = anim_clip_data[go];
            copy_anim_channel_data_vec3(out_channel.translation, in_channel.channel_translation);
            copy_anim_channel_data_vec3(out_channel.scale, in_channel.channel_scale);
            copy_anim_channel_data_quat(out_channel.rotation, in_channel.channel_rotation);
        }
        ret.anim_controller.add_animation_clip(anim_in.name, project_c::AnimationClip(scene, std::move(anim_clip_data)));
    }

    return ret;
}

bool project_c::Prefab::is_valid() const
{
    return model_info_.nodes_count > 0;
}
