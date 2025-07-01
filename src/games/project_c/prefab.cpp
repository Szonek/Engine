#include "prefab.h"

#include <format>
#include <iscene.h>

project_c::Prefab::Prefab(Prefab&& rhs) noexcept
{
    std::swap(app_, rhs.app_);
    std::swap(model_desc_, rhs.model_desc_);
    std::swap(geometries_, rhs.geometries_);
    std::swap(textures_, rhs.textures_);
    std::swap(materials_, rhs.materials_);
    std::swap(skins_, rhs.skins_);
}

project_c::Prefab& project_c::Prefab::operator=(Prefab&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::swap(app_, rhs.app_);
        std::swap(model_desc_, rhs.model_desc_);
        std::swap(geometries_, rhs.geometries_);
        std::swap(textures_, rhs.textures_);
        std::swap(materials_, rhs.materials_);
        std::swap(skins_, rhs.skins_);
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
            if (t.owner)
            {
                engineApplicationDestroyTexture2D(app_, t.obj);
            }
        }
        for (const auto& skin : skins_)
        {
            if (skin)
            {
                engineApplicationDestroySkin(app_, skin);
            }
        }
        materials_.clear();
        engineApplicationReleaseModelDesc(app_, model_desc_);
    }
}

project_c::Prefab::Prefab(engine_result_code_t& engine_error_code, engine_application_t& app, std::string_view model_file_name, std::string_view base_dir)
    : app_(app)
{
    engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, model_file_name.data(), base_dir.data(), &model_desc_);

    geometries_ = std::vector(engineModelDescGetGeometriesDescCount(model_desc_), ENGINE_INVALID_OBJECT_HANDLE);
    for (std::uint32_t i = 0; i < geometries_.size(); i++)
    {
        const auto& geo_desc = engineModelDescGetGeometryDesc(model_desc_, i);
        engine_error_code = engineApplicationCreateGeometryFromDesc(app, geo_desc, &geometries_[i]);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed creating geometry for loaded model. Exiting!\n");
            return;
        }
    }

    textures_ = std::vector<EngineObj<engine_texture2d_t>>(engineModelDescGetTextures2dDescCount(model_desc_));
    for (std::uint32_t i = 0; i < textures_.size(); i++)
    {
        const auto& texture_desc = engineModelDescGetTexture2dDesc(model_desc_, i);
        const auto name_generic = std::string(model_file_name) + "_texture_" + std::to_string(i);
        const auto name_real = engineTexture2dDescGetName(texture_desc);
        const std::string name = name_real ? name_real : name_generic;
        if (engineApplicationDoTexture2DNameExists(app, name.c_str()))
        {
            engineLog(std::format("Texture with name: {} already exists, reusing it.\n", name).c_str());
            textures_[i].obj = engineApplicationGetTextured2DByName(app, name.c_str());
            textures_[i].owner = false;
            engine_error_code = textures_[i].obj == ENGINE_INVALID_OBJECT_HANDLE ? ENGINE_RESULT_CODE_FAIL : ENGINE_RESULT_CODE_OK;
        }
        else
        {
            engine_error_code = engineApplicationCreateTexture2DFromDesc(app, texture_desc, &textures_[i].obj);
            textures_[i].owner = true;
        }

        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed creating texture for loaded model. Exiting!\n");
            return;
        }
    }

    materials_ = std::vector<engine_material_component_t>(engineModelDescGetMaterialsDescCount(model_desc_));
    for (std::uint32_t i = 0; i < materials_.size(); i++)
    {
        const auto& mat_desc = engineModelDescGetMaterialDesc(model_desc_, i);
        auto& mat_comp = materials_.at(i);
        mat_comp.type = ENGINE_MATERIAL_TYPE_PONG;
        set_c_array(mat_comp.data.pong.diffuse_color, engineMaterialDescGetDiffuseColor(mat_desc));
        const auto diffuse_texture_idx = engineMaterialDescGetDiffuseTextureIndex(mat_desc);
        if (diffuse_texture_idx != -1)
        {
            mat_comp.data.pong.diffuse_texture = textures_.at(diffuse_texture_idx).obj;
        }
        mat_comp.data.pong.shininess = 32;
    }

    skins_ = std::vector<engine_skin_t*>(engineModelDescGetSkinsDescCount(model_desc_));
    for (std::uint32_t i = 0; i < skins_.size(); i++)
    {
        const auto& skin_desc = engineModelDescGetSkinDesc(model_desc_, i);
        const auto skin_name = engineSkinDescGetName(skin_desc);
        const auto joints_count = engineSkinDescGetJointsCount(skin_desc);
        // find root node for the skin
        const engine_model_node_desc_t* root_node_desc = nullptr;
        for (auto j = 0; j < joints_count; j++)
        {
            const auto joint_name = engineSkinDescGetJointName(skin_desc, j);
            const auto node_desc = engineModelDescGetNodeDescByName(model_desc_, joint_name);
            if (!root_node_desc || engineModelNodeDescGetIndex(node_desc) > engineModelNodeDescGetIndex(root_node_desc))
            {
                root_node_desc = node_desc;
            }
        }
        if (root_node_desc)
        {
            skins_[i] = engineApplicationCreateSkinFromDesc(app, skin_desc, root_node_desc);
        }

        if (!skins_[i])
        {
            engineLog("Failed creating skin for loaded model. Exiting!\n");
            //return;
        }
    }
}

project_c::PrefabResult project_c::Prefab::instantiate(engine::IScene* scene_cpp) const
{

    auto scene = scene_cpp->get_handle();
    project_c::PrefabResult ret{};
    ret.go = ENGINE_INVALID_GAME_OBJECT_ID;

    std::map<std::uint32_t, engine_game_object_t> node_id_to_game_object;
    const auto nodes_count = engineModelDescGetNodesDescCount(model_desc_);
    for (auto i = 0; i < nodes_count; i++)
    {
        const auto node_desc = engineModelDescGetNodeDesc(model_desc_, i);
        const auto& go = engineSceneCreateGameObject(scene);
        node_id_to_game_object[engineModelNodeDescGetIndex(node_desc)] = go;
        const auto name = engineModelNodeDescGetName(node_desc);
        if (name)
        {
            auto nc = engineSceneAddNameComponent(scene, go);
            std::strncpy(nc.name, name, std::size(nc.name));
            engineSceneUpdateNameComponent(scene, go, &nc);

        }
        log(std::format("Created entity [id: {}] with name: {}\n", go, name));

        for (const auto& skin : skins_)
        {
            const auto& skin_name = engineSkinGetName(skin);
            if (std::strcmp(skin_name, name) == 0)
            {
                auto sc = engineSceneAddSkinComponent(scene, go);
                sc.skin = skin;
                engineSceneUpdateSkinComponent(scene, go, &sc);
                log(std::format("\t[{}] has added skin component with name: {}\n", go, skin_name));
            }
        }

        // transform
        {
            auto tc = engineSceneAddTransformComponent(scene, go);
            set_c_array(tc.position, engineModelNodeDescGetTranslation(node_desc));
            set_c_array(tc.rotation, engineModelNodeDescGetRotationQuaternion(node_desc));
            set_c_array(tc.scale, engineModelNodeDescGetScale(node_desc));
            engineSceneUpdateTransformComponent(scene, go, &tc);
            log(std::format("\t[{}] has added transform component\n", go));
        }

        const auto skin_index = engineModelNodeDescGetSkinIndex(node_desc);
        const auto geo_index = engineModelNodeDescGetGeometryIndex(node_desc);
        if (geo_index != -1)
        {
            if (skin_index != -1)
            {
                auto smc = engineSceneAddSkinnedMeshComponent(scene, go);
                smc.geometry = geometries_.at(geo_index);
                smc.skin = skins_.at(skin_index);
                engineSceneUpdateSkinnedMeshComponent(scene, go, &smc);
                log(std::format("\t[{}] has added skinned mesh component with geometry index: {} and skin index: {}\n", go, geo_index, skin_index));
            }
            else
            {
                auto mc = engineSceneAddMeshComponent(scene, go);
                mc.geometry = geometries_.at(geo_index);
                engineSceneUpdateMeshComponent(scene, go, &mc);
                log(std::format("\t[{}] has added mesh component with geometry index: {}\n", go, geo_index));
            }

        }

        const auto mat_index = engineModelNodeDescGetMaterialIndex(node_desc);
        if (mat_index != -1)
        {
            auto material = engineSceneAddMaterialComponent(scene, go);
            material = materials_.at(mat_index);
            engineSceneUpdateMaterialComponent(scene, go, &material);
            log(std::format("\t[{}] added material component with material idx: {}\n", go, mat_index));
        }

        if (!engineModelNodeDescGetParent(node_desc))
        {
            ret.go = go;
        }
    }
    assert(ret.go != ENGINE_INVALID_GAME_OBJECT_ID);

    // hierarchy
    for (auto i = 0; i < nodes_count; i++)
    {
        const auto node_desc = engineModelDescGetNodeDesc(model_desc_, i);
        const auto& go = node_id_to_game_object[engineModelNodeDescGetIndex(node_desc)];
        if (const auto parent_node_desc = engineModelNodeDescGetParent(node_desc))
        {
            const std::uint32_t parent_index = engineModelNodeDescGetIndex(parent_node_desc);
            // add parent component
            auto pc = engineSceneAddParentComponent(scene, go);
            pc.parent = node_id_to_game_object[parent_index];
            engineSceneUpdateParentComponent(scene, go, &pc);
            log(std::format("Entity: {} added parent component: {}\n", go, pc.parent));
        }
    }

    //// bones
    //std::map<uint32_t, std::vector<engine_game_object_t>> skin_to_game_object;
    //for (auto skin_idx = 0; skin_idx < model_info_.skins_counts; skin_idx++)
    //{
    //    if (skin_idx == 1)
    //    {
    //        break;
    //    }
    //    const auto& skin = model_info_.skins_array[skin_idx];
    //    log(std::format("Adding skin: {}\n", skin.name));
    //    for (auto bone_idx = 0; bone_idx < skin.bones_count; bone_idx++)
    //    {
    //        const auto& bone = skin.bones_array[bone_idx];
    //        const auto& go = node_id_to_game_object[bone.model_node_index];
    //        skin_to_game_object[skin_idx].push_back(go);

    //        auto bc = engineSceneAddBoneComponent(scene, go);
    //        std::memcpy(bc.inverse_bind_matrix, bone.inverse_bind_mat, sizeof(bc.inverse_bind_matrix));
    //        engineSceneUpdateBoneComponent(scene, go, &bc);
    //        log(std::format("\tAttached entity: {} to the skin.\n", go));
    //    }
    //}

    // update nodes with skin components
    //for (auto i = 0; i < model_info_.nodes_count; i++)
    //{
    //    const auto& node = model_info_.nodes_array[i];
    //    const auto& go = node_id_to_game_object[i];
    //    auto skin_index = node.skin_index;
    //    if (skin_index != -1)
    //    {
    //        skin_index = 0;
    //        const auto& bones_game_object_arr = skin_to_game_object[skin_index];
    //        auto sc = engineSceneAddSkinComponent(scene, go);
    //        for (auto bone_idx = 0; bone_idx < bones_game_object_arr.size(); bone_idx++)
    //        {
    //            sc.bones[bone_idx] = bones_game_object_arr.at(bone_idx);
    //        }
    //        engineSceneUpdateSkinComponent(scene, go, &sc);
    //        log(std::format("Entity: {} added skin component for skin index: \n", go, skin_index));
    //    }
    //}

    //// animations
    //auto copy_anim_channel_data_vec3 = [](project_c::AnimationChannelVec3& out_channel, const engine_animation_channel_data_t& in_channel)
    //    {
    //        //timestamps
    //        out_channel.timestamps.resize(in_channel.timestamps_count);
    //        std::memcpy(out_channel.timestamps.data(), in_channel.timestamps, in_channel.timestamps_count * sizeof(in_channel.timestamps[0]));

    //        // data
    //        out_channel.data.resize(in_channel.data_count / project_c::AnimationChannelVec3::DataType::length());
    //        std::memcpy(out_channel.data.data(), in_channel.data, in_channel.data_count * sizeof(in_channel.data[0]));
    //    };

    //auto copy_anim_channel_data_quat = [](project_c::AnimationChannelQuat& out_channel, const engine_animation_channel_data_t& in_channel)
    //    {
    //        //timestamps
    //        out_channel.timestamps.resize(in_channel.timestamps_count);
    //        std::memcpy(out_channel.timestamps.data(), in_channel.timestamps, in_channel.timestamps_count * sizeof(in_channel.timestamps[0]));

    //        // data
    //        out_channel.data.resize(in_channel.data_count / project_c::AnimationChannelQuat::DataType::length());
    //        std::memcpy(out_channel.data.data(), in_channel.data, in_channel.data_count * sizeof(in_channel.data[0]));
    //    };

    //ret.anim_controller.set_scene(scene);
    //for (auto anim_idx = 0; anim_idx < model_info_.animations_counts; anim_idx++)
    //{
    //    const auto& anim_in = model_info_.animations_array[anim_idx];
    //    log(std::format("Adding animation: {}\n", anim_in.name));
    //    std::map<engine_game_object_t, project_c::AnimationChannelData> anim_clip_data;
    //    for (auto channel_idx = 0; channel_idx < anim_in.channels_count; channel_idx++)
    //    {
    //        const auto& in_channel = anim_in.channels[channel_idx];
    //        const auto& go = node_id_to_game_object[in_channel.model_node_index];
    //        assert(anim_clip_data.find(go) == anim_clip_data.end());
    //        project_c::AnimationChannelData& out_channel = anim_clip_data[go];
    //        copy_anim_channel_data_vec3(out_channel.translation, in_channel.channel_translation);
    //        copy_anim_channel_data_vec3(out_channel.scale, in_channel.channel_scale);
    //        copy_anim_channel_data_quat(out_channel.rotation, in_channel.channel_rotation);
    //    }
    //    ret.anim_controller.add_animation_clip(anim_in.name, project_c::AnimationClip(std::move(anim_clip_data)));
    //}
    return ret;
}

bool project_c::Prefab::is_valid() const
{
    return model_desc_ && engineModelDescGetNodesDescCount(model_desc_) > 0;
}
