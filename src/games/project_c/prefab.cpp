#include <iscene.h>
#include "prefab.h"

#include "scripts/scripts_utils.h"

#include <map>
#include <format>

project_c::Prefab::Prefab(Prefab&& rhs) noexcept
{
    std::swap(model_desc_, rhs.model_desc_);
    std::swap(geometries_, rhs.geometries_);
    std::swap(textures_, rhs.textures_);
    std::swap(materials_, rhs.materials_);
    std::swap(animation_controllers_, rhs.animation_controllers_);
}

project_c::Prefab& project_c::Prefab::operator=(Prefab&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::swap(model_desc_, rhs.model_desc_);
        std::swap(geometries_, rhs.geometries_);
        std::swap(textures_, rhs.textures_);
        std::swap(materials_, rhs.materials_);
        std::swap(skins_, rhs.skins_);
        std::swap(animation_controllers_, rhs.animation_controllers_);
    }
    return *this;
}

project_c::Prefab::~Prefab()
{
    if (is_valid())
    {
        for (const auto& g : geometries_)
        {
            engineDestroyGeometry(g);
        }
        for (const auto& t : textures_)
        {
            if (t.owner)
            {
                engineDestroyTexture2D(t.obj);
            }
        }
        for (const auto& skin : skins_)
        {
            if (skin)
            {
                engineDestroySkin(skin);
            }
        }
        for (const auto& anim_controller : animation_controllers_)
        {
            if (anim_controller)
            {
                engineDestroyAnimationController(anim_controller);
            }
        }
        materials_.clear();
        engineReleaseModelDesc(model_desc_);
    }
}

project_c::Prefab::Prefab(engine_result_code_t& engine_error_code, std::string_view model_file_name, std::string_view base_dir)
{
    engine_error_code = engineAllocateModelDescAndLoadDataFromFile(ENGINE_MODEL_SPECIFICATION_GLTF_2, model_file_name.data(), base_dir.data(), &model_desc_);

    geometries_ = std::vector(engineModelDescGetGeometriesDescCount(model_desc_), ENGINE_INVALID_OBJECT_HANDLE);
    for (std::uint32_t i = 0; i < geometries_.size(); i++)
    {
        const auto& geo_desc = engineModelDescGetGeometryDesc(model_desc_, i);
        engine_error_code = engineCreateGeometryFromDesc(geo_desc, &geometries_[i]);
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
        if (engineDoTexture2DNameExists(name.c_str()))
        {
            engineLog(std::format("Texture with name: {} already exists, reusing it.\n", name).c_str());
            textures_[i].obj = engineGetTextured2DByName(name.c_str());
            textures_[i].owner = false;
            engine_error_code = textures_[i].obj == ENGINE_INVALID_OBJECT_HANDLE ? ENGINE_RESULT_CODE_FAIL : ENGINE_RESULT_CODE_OK;
        }
        else
        {
            engine_error_code = engineCreateTexture2DFromDesc(texture_desc, &textures_[i].obj);
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
}

project_c::PrefabResult project_c::Prefab::instantiate(engine::IScene* scene_cpp)
{
    auto scene = scene_cpp->get_handle();
    
    // Set the scene as active for the new non-context API
    engineSetActiveScene(scene);

    project_c::PrefabResult ret{};
    ret.go = ENGINE_INVALID_GAME_OBJECT_ID;

    const auto skins_count = engineModelDescGetSkinsDescCount(model_desc_);
    assert(skins_count <= 1); // only non-skinned or single-skin models allowed
    engine_skin_t* skin_handle = nullptr;
    std::string skin_name = "";
    // skin
    if (skins_count == 1)
    {
        const auto& skin_desc = engineModelDescGetSkinDesc(model_desc_, 0);
        skin_name = engineSkinDescGetName(skin_desc);
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
        if (!root_node_desc)
        {
            engineLog("Failed to find root node for the skin. Exiting!\n");
            return ret;
        }
        skin_handle = engineCreateSkinFromDesc(skin_desc, root_node_desc);
        if (!skin_handle)
        {
            engineLog("Failed creating skin for loaded model. Exiting!\n");
            return ret;
        }
    }

    engine_animation_controller_t* anim_controller = nullptr;
    const auto animations_count = engineModelDescGetAnimationsDescCount(model_desc_);
    if (animations_count > 0)
    {
        // create animation controller
        anim_controller = engineCreateAnimationControllerWithSkin(skin_handle);
        if (!anim_controller)
        {
            engineLog("Failed creating animation controller for loaded model. Exiting!\n");
            return ret;
        }
        // add animations to the controller
        for (auto i = 0; i < animations_count; i++)
        {
            const auto& anim_desc = engineModelDescGetAnimationDesc(model_desc_, i);
            const auto anim_name = engineAnimationDescGetName(anim_desc);
            if (!engineAnimationControllerAddAnimation(anim_controller, anim_desc))
            {
                engineLog(std::format("Failed adding animation: {} to the controller. Exiting!\n", anim_name).c_str());
                engineDestroyAnimationController(anim_controller);
                return ret;
            }
        }
        animation_controllers_.push_back(anim_controller);
    }

    std::map<std::uint32_t, engine_game_object_t> node_id_to_game_object;
    const auto nodes_count = engineModelDescGetNodesDescCount(model_desc_);
    for (auto i = 0; i < nodes_count; i++)
    {
        const auto node_desc = engineModelDescGetNodeDesc(model_desc_, i);
        const auto& go = engineCreateGameObject();
        node_id_to_game_object[engineModelNodeDescGetIndex(node_desc)] = go;
        const auto name = engineModelNodeDescGetName(node_desc);
        if (name)
        {
            auto nc = engineAddNameComponent(go);
            std::strncpy(nc.name, name, std::size(nc.name));
            engineUpdateNameComponent(go, &nc);

        }
        log(std::format("Created entity [id: {}] with name: {}\n", go, name));

        // skin
        if (skin_name == std::string(name))
        {
            // node with the same name as skin will be owner of the skin
            if (skin_handle)
            {
                auto sc = engineSceneAddSkinComponent(scene, go);
                sc.skin = skin_handle;
                engineSceneUpdateSkinComponent(scene, go, &sc);
                log(std::format("\t[{}] has added skin component with name: {}\n", go, skin_name));
                skins_.push_back(skin_handle);
            }

            // and owner of the animation controller
            if (anim_controller)
            {
                auto ac = engineSceneAddAnimationControllerComponent(scene, go);
                ac.controller = anim_controller;
                engineSceneUpdateAnimationControllerComponent(scene, go, &ac);
                log(std::format("\t[{}] has added animation controller component with name: {}\n", go, skin_name));
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
                if (!skin_handle)
                {
                    engineLog("Failed creating skin for loaded model. Exiting!\n");
                    assert(false);
                }
                auto smc = engineSceneAddSkinnedMeshComponent(scene, go);
                smc.geometry = geometries_.at(geo_index);
                smc.skin = skin_handle;
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

    // remove not needed objects (joints)
    for (auto i = 0; i < skins_count; i++)
    {
        const auto& skin_desc = engineModelDescGetSkinDesc(model_desc_, i);
        for (auto j = 0; j < engineSkinDescGetJointsCount(skin_desc); j++)
        {
            const auto joint_name = engineSkinDescGetJointName(skin_desc, j);
            const auto go = utils::get_game_objects_with_name(scene, joint_name).at(0);
            engineSceneDestroyGameObject(scene, go);

            const auto model_node_desc = engineModelDescGetNodeDescByName(model_desc_, joint_name);
            node_id_to_game_object.erase(engineModelNodeDescGetIndex(model_node_desc));

            // but objects which parent was joint have to have added joint attachment
            for (auto c = 0; c < engineModelNodeDescGetChildrenCount(model_node_desc); c++)
            {
                const auto child = engineModelNodeDescGetChildren(model_node_desc, c);
                const auto child_has_geometry = engineModelNodeDescGetGeometryIndex(child) != -1;
                if (child_has_geometry)
                {
                    const auto child_go = node_id_to_game_object.at(engineModelNodeDescGetIndex(child));
                    auto jac = engineSceneAddJointAttachmentComponent(scene, child_go);
                    assert(skin_handle != nullptr);
                    jac.skin = skin_handle;
                    engineStringSet(jac.joint_name, joint_name);
                    engineSceneUpdateJointAttachmentComponent(scene, child_go, &jac);

                    // reset transform (it is not needed, since it will be picked from joint.
                    auto tc = engineSceneGetTransformComponent(scene, child_go);
                    set_c_array(tc.position, engine_fvec3_t{ 0.0f, 0.0f, 0.0f});
                    set_c_array(tc.scale, engine_fvec3_t{ 1.0f, 1.0f, 1.0f });
                    set_c_array(tc.rotation, engine_fvec4_t{ 0.0f, 0.0f, 0.0f, 1.0f });
                    engineSceneUpdateTransformComponent(scene, child_go, &tc);
                }
            }
        }
    }


    // all objects are children of the return object
    for (const auto& [node_id, go] : node_id_to_game_object)
    {
        if (go == ret.go)
        {
            continue;
        }
        auto pc = engineSceneAddParentComponent(scene, go);
        pc.parent = ret.go;
        engineSceneUpdateParentComponent(scene, go, &pc);
    }

    return ret;
}

bool project_c::Prefab::is_valid() const
{
    return model_desc_ && engineModelDescGetNodesDescCount(model_desc_) > 0;
}
