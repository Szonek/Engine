#include <engine.h>

#include "scene_manager.h"
#include "iscene.h"
#include "utils.h"
#include "iscript.h"
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
struct ModelInfo
{
    engine_application_t& app;
    engine_model_desc_t model_info{};
    std::vector<engine_geometry_t> geometries;
    std::vector<engine_texture2d_t> textures;
    std::vector<engine_material_t> materials;
    std::vector<engine_skin_t> skins;
    std::vector<engine_animation_clip_t> animations;

    ModelInfo(engine_result_code_t& engine_error_code, engine_application_t& app, std::string_view model_file_name)
        : app(app)
    {

        engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, model_file_name.data(), &model_info);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed loading TABLE model. Exiting!\n");
            return;
        }

        geometries = std::vector(model_info.geometries_count, ENGINE_INVALID_OBJECT_HANDLE);
        for (std::uint32_t i = 0; i < model_info.geometries_count; i++)
        {
            const auto& geo = model_info.geometries_array[i];
            const auto name = "unnamed_geometry__" + std::to_string(i);
            engine_error_code = engineApplicationAddGeometryFromDesc(app, &geo, name.c_str(), &geometries[i]);
            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating geometry for loaded model. Exiting!\n");
                return;
            }
        }

        textures = std::vector<engine_texture2d_t>(model_info.textures_count, ENGINE_INVALID_OBJECT_HANDLE);
        for (std::uint32_t i = 0; i < model_info.textures_count; i++)
        {
            const auto name = "unnamed_texture_" + std::to_string(i);
            engine_error_code = engineApplicationAddTexture2DFromDesc(app, &model_info.textures_array[i], name.c_str(), &textures[i]);
            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating texture for loaded model. Exiting!\n");
                return;
            }
        }

        materials = std::vector<engine_material_t>(model_info.materials_count, ENGINE_INVALID_OBJECT_HANDLE);
        for (std::uint32_t i = 0; i < model_info.materials_count; i++)
        {
            const auto& mat = model_info.materials_array[i];
            engine_material_create_desc_t mat_create_desc{};
            set_c_array(mat_create_desc.diffuse_color, mat.diffuse_color);
            if (mat.diffuse_texture_index != -1)
            {
                mat_create_desc.diffuse_texture = textures.at(mat.diffuse_texture_index);
            }
            engine_error_code = engineApplicationAddMaterialFromDesc(app, &mat_create_desc, mat.name, &materials[i]);

            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating textured for loaded model. Exiting!\n");
                return;
            }
        }

        //skins = std::vector<engine_skin_t>(model_info.skins_counts, ENGINE_INVALID_OBJECT_HANDLE);
        //for (auto i = 0; i < model_info.skins_counts; i++)
        //{
        //    const auto& skin = model_info.skins_array[i];
        //    const auto name = "unnamed_skin_" + std::to_string(i);
        //    engine_error_code = engineApplicationAddSkinFromDesc(app, &skin, name.c_str(), &skins[i]);
        //    if (engine_error_code != ENGINE_RESULT_CODE_OK)
        //    {
        //        engineLog("Failed creating textured for loaded model. Exiting!\n");
        //        return;
        //    }
        //}

        animations = std::vector<engine_animation_clip_t>(model_info.animations_counts, ENGINE_INVALID_OBJECT_HANDLE);
        for (auto i = 0; i < model_info.animations_counts; i++)
        {
            const auto& anim = model_info.animations_array[i];
            const auto name = "unnamed_animation_" + std::to_string(i);
            engine_error_code = engineApplicationAddAnimationClipFromDesc(app, &anim, name.c_str(), &animations[i]);
            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating textured for loaded model. Exiting!\n");
                return;
            }
        }
    }

    ~ModelInfo()
    {
        if (model_info.nodes_count > 0)
        {
            engineApplicationReleaseModelDesc(app, &model_info);
        }
    }
};

class CameraScript : public engine::IScript
{
public:
    CameraScript(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        auto camera_comp = engineSceneAddCameraComponent(scene, go_);
        camera_comp.enabled = true;
        camera_comp.clip_plane_near = 0.1f;
        camera_comp.clip_plane_far = 1000.0f;
        camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
        camera_comp.type_union.perspective_fov = 45.0f;

        camera_comp.target[0] = 0.0f;
        camera_comp.target[1] = 0.0f;
        camera_comp.target[2] = 0.0f;

        engineSceneUpdateCameraComponent(scene, go_, &camera_comp);

        auto camera_transform_comp = engineSceneAddTransformComponent(scene, go_);
        camera_transform_comp.position[0] = -2.273151f;
        camera_transform_comp.position[1] = 4.3652916f;
        camera_transform_comp.position[2] = 1.3330482f;
        engineSceneUpdateTransformComponent(scene, go_, &camera_transform_comp);

        sc_ = get_spherical_coordinates(camera_transform_comp.position);
    }

    void update(float dt) override
    {
        const auto app = my_scene_->get_app_handle();
        const auto mouse_coords = engineApplicationGetMouseCoords(app);

        const auto dx = mouse_coords.x - mouse_coords_prev_.x;
        const auto dy = mouse_coords.y - mouse_coords_prev_.y;

        if (mouse_coords.x != mouse_coords_prev_.x || mouse_coords.y != mouse_coords_prev_.y)
        {
            mouse_coords_prev_ = mouse_coords;
        }

        const float move_speed = 1.0f * dt;

        if (engineApplicationIsMouseButtonDown(app, engine_mouse_button_t::ENGINE_MOUSE_BUTTON_LEFT))
        {
            rotate({ dx * move_speed, dy * move_speed });
        }

        if (engineApplicationIsMouseButtonDown(app, engine_mouse_button_t::ENGINE_MOUSE_BUTTON_RIGHT))
        {
            // zoom in/out with right mouse button 
            // update spherical coordinates (radius -> sc_[0])
            translate({ 0.0f, 0.0f, dy * move_speed });
        }
    }

private:
    inline void translate(const glm::vec3& delta)
    {
        const auto scene = my_scene_->get_handle();
        // Decrease the radius based on the delta's z value
        sc_[0] -= delta.z;
        // Make sure the radius doesn't go below a certain threshold to prevent the camera from going inside the target
        sc_[0] = std::max(sc_[0], 0.1f);
        // Update the camera's position based on the new spherical coordinates
        const auto new_position = get_cartesian_coordinates(sc_);
        auto tc = engineSceneGetTransformComponent(scene, go_);
        auto cc = engineSceneGetCameraComponent(scene, go_);
        tc.position[0] = new_position[0] + cc.target[0];
        tc.position[1] = new_position[1] + cc.target[1];
        tc.position[2] = new_position[2] + cc.target[2];
        engineSceneUpdateTransformComponent(scene, go_, &tc);
    }

    inline void rotate(const glm::vec2 delta)
    {
        const auto scene = my_scene_->get_handle();
        // https://nerdhut.de/2020/05/09/unity-arcball-camera-spherical-coordinates/
        if (delta.x != 0 || delta.y != 0)
        {
            auto tc = engineSceneGetTransformComponent(scene, go_);
            // Rotate the camera left and right
            sc_[1] += delta.x;

            // Rotate the camera up and down
            // Prevent the camera from turning upside down (1.5f = approx. Pi / 2)
            sc_[2] = std::clamp(sc_[2] + delta.y, -1.5f, 1.5f);

            const auto new_position = get_cartesian_coordinates(sc_);
            auto cc = engineSceneGetCameraComponent(scene, go_);
            tc.position[0] = new_position[0] + cc.target[0];
            tc.position[1] = new_position[1] + cc.target[1];
            tc.position[2] = new_position[2] + cc.target[2];
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
    }

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

private:
    std::array<float, 3> sc_;  // {radius, phi, theta}
    engine_coords_2d_t mouse_coords_prev_{};
};

struct SkinInfo
{
    std::map<std::uint32_t, engine_game_object_t> bones;
};

class BaseNode : public engine::IScript
{
public:
    BaseNode(engine::IScene* my_scene, const engine_model_node_desc_t& node, const ModelInfo& model_info)
        : IScript(my_scene)
    {
        const auto scene = my_scene_->get_handle();
        auto nc = engineSceneAddNameComponent(scene, go_);
        if (node.name)
        {
            std::strncpy(nc.name, node.name, std::max(std::strlen(node.name), std::size(nc.name)));
            if (std::strlen(node.name) > std::size(nc.name))
            {
                log(fmt::format("Couldnt copy full entity name. Orginal name: {}, entity name: {}!\n", nc.name, node.name));
            }
            engineSceneUpdateNameComponent(scene, go_, &nc);
        }
        log(fmt::format("Created entity [id: {}] with name: {}\n", go_, nc.name));

        auto tc = engineSceneAddTransformComponent(scene, go_);
        std::memcpy(tc.position, node.translate, std::size(node.translate) * sizeof(float));
        std::memcpy(tc.scale, node.scale, std::size(node.scale) * sizeof(float));
        std::memcpy(tc.rotation, node.rotation_quaternion, std::size(node.rotation_quaternion) * sizeof(float));
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        if (node.geometry_index != -1 || node.skin_index != -1)
        {
            auto mc = engineSceneAddMeshComponent(scene, go_);
            mc.geometry = node.geometry_index != -1 ? model_info.geometries[node.geometry_index] : ENGINE_INVALID_OBJECT_HANDLE;
            mc.skin = node.skin_index != -1 ? model_info.skins[node.skin_index] : ENGINE_INVALID_OBJECT_HANDLE;
            engineSceneUpdateMeshComponent(scene, go_, &mc);
        }

        if (node.bone_index != -1)
        {
            auto bc = engineSceneAddBoneComponent(scene, go_);
            const auto bone_info = model_info.model_info.bones_array[node.bone_index];
            std::memcpy(bc.inverse_bind_matrix, bone_info.inverse_bind_mat, sizeof(bone_info.inverse_bind_mat));
            engineSceneUpdateBoneComponent(scene, go_, &bc);
        }
        //if (node.skin_index != -1)
        //{
        //    auto anim_comp = engineSceneAddAnimationComponent(scene, go_);
        //    const auto skin_info = model_info.model_info.skins_array[node.skin_index];
        //    for (auto i = 0; i < skin_info.animations_count; i++)
        //    {
        //        anim_comp.animations_array[i] = model_info.animations.at(skin_info.animations_array[i]);
        //    }
        //    engineSceneUpdateAnimationComponent(scene, go_, &anim_comp);
        //}

        // if mesh is present then definitly we need some material to render it
        if (node.material_index != -1)
        {
            auto material_comp = engineSceneAddMaterialComponent(scene, go_);
            material_comp.material = model_info.materials.at(node.material_index);
            engineSceneUpdateMaterialComponent(scene, go_, &material_comp);
        }
    }

    virtual void try_to_find_and_set_parent_entity(const engine_model_node_desc_t& node, const ModelInfo& model_info)
    {
        const auto scene = my_scene_->get_handle();
        // this is potentaiyll very costly part of this c-tor
        if (node.parent)
        {
            engine_component_view_t view = nullptr;
            engineCreateComponentView(&view);
            engineSceneComponentViewAttachNameComponent(scene, view);

            engine_component_iterator_t begin{};
            engine_component_iterator_t end{};
            engineComponentViewCreateBeginComponentIterator(view, &begin);
            engineComponentViewCreateEndComponentIterator(view, &end);

            engine_game_object_t parent_go = ENGINE_INVALID_GAME_OBJECT_ID;
            while (!engineComponentIteratorCheckEqual(begin, end))
            {
                const auto go = engineComponentIteratorGetGameObject(begin);
                const auto go_name_comp = engineSceneGetNameComponent(scene, go);
                if (std::strcmp(go_name_comp.name, node.parent->name) == 0)
                {
                    parent_go = go;
                    begin = end; // finish
                }
                else
                {
                    engineComponentIteratorNext(begin);
                }

            }

            if (parent_go != ENGINE_INVALID_GAME_OBJECT_ID)
            {
                auto pc = engineSceneAddParentComponent(scene, go_);
                pc.parent = parent_go;
                engineSceneUpdateParentComponent(scene, go_, &pc);
            }
            if (view)
            {
                engineDestroyComponentView(view);
            }
        }
    }
};

class Node3D : public BaseNode
{
public:
    Node3D(engine::IScene* my_scene, const engine_model_node_desc_t& node, const ModelInfo& model_info)
        : BaseNode(my_scene, node, model_info)
    {
    }
};

class Floor : public BaseNode
{
public:
    Floor(engine::IScene* my_scene, const engine_model_node_desc_t& node, const ModelInfo& model_info)
        : BaseNode(my_scene, node, model_info)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.scale[0] = 3.0f;
        tc.scale[1] = 0.1f;
        tc.scale[2] = 3.0f;

        tc.position[1] -= 0.25f;

        const auto q = glm::rotate(glm::make_quat(tc.rotation), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        for (auto i = 0; i < q.length(); i++)
        {
            tc.rotation[i] = q[i];
        }


        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // material
        engine_material_create_desc_t mat_cd{};
        set_c_array(mat_cd.diffuse_color, std::array<float, 4>({0.23, 0.7, 0.44f, 1.0f}));
        engine_material_t mat{};
        if (engineApplicationAddMaterialFromDesc(app, &mat_cd, "floor_material", &mat) == ENGINE_RESULT_CODE_OK)
        {
            auto mc = engineSceneGetMaterialComponent(scene, go_);
            mc.material = mat;
            engineSceneUpdateMaterialComponent(scene, go_, &mc);
        }

        // physcis
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_BOX;
        set_c_array(cc.collider.box.size, std::array<float, 3>{ 1.0f, 1.0f, 1.0f }); 
        engineSceneUpdateColliderComponent(scene, go_, &cc);
    }
};


class Enemy : public BaseNode
{
public:
    Enemy(engine::IScene* my_scene, const engine_model_node_desc_t& node, const ModelInfo& model_info)
        : BaseNode(my_scene, node, model_info)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.75f;
        tc.scale[2] = 0.5f;

        tc.position[0] += 1.0f;
        tc.position[1] += 2.5f;
        tc.position[2] += 0.0f;
        
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // physcis
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_BOX;
        set_c_array(cc.collider.box.size, std::array<float, 3>{ 1.0f, 1.0f, 1.0f});
        engineSceneUpdateColliderComponent(scene, go_, &cc);

        //rb
        auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
        rbc.mass = 1.0f;
        engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
    }
};

//class ControllableEntity : public BaseNode
class ControllableEntity : public engine::IScript
{
public:
    ControllableEntity(engine::IScene* my_scene, const engine_tranform_component_t& transform,
        engine_geometry_t geometry, engine_material_t material, engine_skin_t skin, const std::vector<engine_animation_clip_t>& anims)
        : IScript(my_scene)
    {
        const auto scene = my_scene->get_handle();
        const auto app = my_scene_->get_app_handle();
        // ------------ transform
        auto tc = engineSceneAddTransformComponent(scene, go_);
        std::memcpy(&tc, &transform, sizeof(engine_tranform_component_t));
        auto quat_rot90y = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        const auto quat = quat_rot90y * glm::make_quat(tc.rotation);
        for (int i = 0; i < quat.length(); i++)
        {
            tc.rotation[i] = quat[i];
        }
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // ------------ rendering
        if (geometry != ENGINE_INVALID_OBJECT_HANDLE)
        {
            auto mc = engineSceneAddSkinnedMeshComponent(scene, go_);
            mc.geometry = geometry;
            //mc.skeleton
            engineSceneUpdateSkinnedMeshComponent(scene, go_, &mc);
        }

        // if mesh is present then definitly we need some material to render it
        if (material != ENGINE_INVALID_OBJECT_HANDLE)
        {
            auto material_comp = engineSceneAddMaterialComponent(scene, go_);
            material_comp.material = material;
            engineSceneUpdateMaterialComponent(scene, go_, &material_comp);
        }

        if (skin != ENGINE_INVALID_OBJECT_HANDLE && !anims.empty())
        {
            auto anim_comp = engineSceneAddAnimationComponent(scene, go_);
            for (auto i = 0; i < anims.size(); i++)
            {
                anim_comp.animations_array[i] = anims[i];
            }
            engineSceneUpdateAnimationComponent(scene, go_, &anim_comp);
        }

        // ------------ physcis
        // collider
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
        cc.collider.compound.children[0].type = ENGINE_COLLIDER_TYPE_BOX;
        const auto position_limits = engineApplicationGeometryGetAttributeLimits(app, geometry, ENGINE_VERTEX_ATTRIBUTE_TYPE_POSITION);
        std::array<float, 3> aabb_center{};
        std::array<float, 3> aabb_half_extent{};
        for (auto i = 0; i < aabb_half_extent.size(); i++)
        {
            aabb_center[i] = (position_limits.min[i] + position_limits.max[i]) / 2.0f;
            aabb_half_extent[i] = (std::abs(position_limits.min[i]) + std::abs(position_limits.max[i])) / 2.0f;
        }
        set_c_array(cc.collider.compound.children[0].rotation_quaternion, std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
        set_c_array(cc.collider.compound.children[0].collider.box.size, aabb_half_extent);
        set_c_array(cc.collider.compound.children[0].transform, aabb_center);
        engineSceneUpdateColliderComponent(scene, go_, &cc);

        // rigid-body
        auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
        rbc.mass = 1.0f;      
        engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
    }

    void update(float dt)
    {
        
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        if (engineSceneHasAnimationComponent(scene, go_))
        {
            auto anim_comp = engineSceneGetAnimationComponent(scene, go_);
            if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_F) && anim_comp.animations_state[0] == ENGINE_ANIMATION_CLIP_STATE_NOT_PLAYING)
            {
                anim_comp.animations_state[0] = ENGINE_ANIMATION_CLIP_STATE_PLAYING;
                engineSceneUpdateAnimationComponent(scene, go_, &anim_comp);
            }
        }



        const float speed = 0.005f * dt;
#if 0
        auto tc = engineSceneGetTransformComponent(scene, go_);
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_W))
        {
            tc.position[0] += speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_S))
        {
            tc.position[0] -= speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_A))
        {
            tc.position[2] -= speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_D))
        {
            tc.position[2] += speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
#else
        auto rbc = engineSceneGetRigidBodyComponent(scene, go_);
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_W))
        {
            rbc.linear_velocity[0] += speed;
            engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_S))
        {
            rbc.linear_velocity[0] -= speed;
            engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_A))
        {
            rbc.linear_velocity[2] -= speed;
            engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_D))
        {
            rbc.linear_velocity[2] += speed;
            engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_SPACE))
        {
            rbc.linear_velocity[1] += 8.0f * speed;
            engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
        }
#endif
    }
};

class TestScene : public engine::IScene
{
public:
    TestScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
        : IScene(app_handle, scn_mgn, engine_error_code)
    {
        auto camera_script = register_script<CameraScript>();
    }

    ~TestScene() = default;
    static constexpr const char* get_name() { return "TestScene"; }
};


inline bool load_controllable_mesh(engine_application_t& app, engine::IScene* scene)
{
    engine_result_code_t engine_error_code = ENGINE_RESULT_CODE_FAIL;
    project_c::ModelInfo model_info(engine_error_code, app, "CesiumMan.gltf");
    //project_c::ModelInfo model_info(engine_error_code, app, "CesiumMan2.glb");
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        return false;
    }
    project_c::SkinInfo skin{};
    // add nodes
    const engine_model_node_desc_t* node_with_geometry = nullptr;
    for (auto i = 0; i < model_info.model_info.nodes_count; i++)
    {
        const auto& node = model_info.model_info.nodes_array[i];
        if (node.geometry_index != ENGINE_INVALID_OBJECT_HANDLE)
        {
            assert(node_with_geometry == nullptr); // only one node with geometry is allowed for now
            node_with_geometry = &node;
        }
        if (node.bone_index != -1)
        {
            scene->register_script<project_c::Node3D>(node, model_info);
        }
    }
    if (!node_with_geometry)
    {
        assert(false && "Failed to find node with geometry!");
        return false;
    }

    glm::vec3 transform = glm::make_vec3(node_with_geometry->translate);
    glm::quat rotation = glm::quat(node_with_geometry->rotation_quaternion[0], node_with_geometry->rotation_quaternion[1], node_with_geometry->rotation_quaternion[2], node_with_geometry->rotation_quaternion[3]);
    glm::vec3 scale = glm::make_vec3(node_with_geometry->scale);

    const engine_model_node_desc_t* parent = node_with_geometry->parent;
    while (parent)
    {
        transform += glm::make_vec3(parent->translate);
        scale *= glm::make_vec3(parent->scale);
        rotation = glm::quat(parent->rotation_quaternion[0], parent->rotation_quaternion[1], parent->rotation_quaternion[2], parent->rotation_quaternion[3]) * rotation;
        parent = parent->parent;  
    }

    engine_tranform_component_t transform_comp{};
    std::memcpy(&transform_comp.position, glm::value_ptr(transform), sizeof(transform_comp.position));
    std::memcpy(&transform_comp.rotation, glm::value_ptr(rotation), sizeof(transform_comp.rotation));
    std::memcpy(&transform_comp.scale, glm::value_ptr(scale), sizeof(transform_comp.scale));
    scene->register_script<project_c::ControllableEntity>(transform_comp,
        model_info.geometries[node_with_geometry->geometry_index],
        model_info.materials[node_with_geometry->material_index],
        model_info.skins.empty() ? ENGINE_INVALID_OBJECT_HANDLE : model_info.skins[node_with_geometry->skin_index],
        model_info.animations);
    return true;
}

inline bool load_cube(engine_application_t& app, engine::IScene* scene)
{
    engine_result_code_t engine_error_code = ENGINE_RESULT_CODE_FAIL;
    const auto load_start = std::chrono::high_resolution_clock::now();
    project_c::ModelInfo model_info(engine_error_code, app, "cube.glb");
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        return false;
    }
    assert(model_info.model_info.nodes_count == 1);
    // add nodes
    const auto& node = model_info.model_info.nodes_array[0];

    // floor
    scene->register_script<project_c::Floor>(node, model_info);
    scene->register_script<project_c::Enemy>(node, model_info);
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
    bool load_model = true;
    load_model = project_c::load_controllable_mesh(app, scene);
    if (!load_model)
    {
        log(fmt::format("Loading model failed!\n"));
        return -1;
    }

    load_model = project_c::load_cube(app, scene);
    if (!load_model)
    {
        log(fmt::format("Loading model failed!\n"));
        return -1;
    }

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