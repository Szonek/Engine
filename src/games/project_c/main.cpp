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


class Floor : public engine::IScript
{
public:
    Floor(engine::IScene* my_scene, const engine_model_node_desc_t& node, const ModelInfo& model_info)
        : engine::IScript(my_scene)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        auto tc = engineSceneAddTransformComponent(scene, go_);
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

        if (node.geometry_index != -1)
        {
            auto mc = engineSceneAddMeshComponent(scene, go_);
            mc.geometry = node.geometry_index != -1 ? model_info.geometries[node.geometry_index] : ENGINE_INVALID_OBJECT_HANDLE;
            engineSceneUpdateMeshComponent(scene, go_, &mc);
        }

        if (node.material_index != -1)
        {
            auto material_comp = engineSceneAddMaterialComponent(scene, go_);
            material_comp.material = model_info.materials.at(node.material_index);
            engineSceneUpdateMaterialComponent(scene, go_, &material_comp);
        }

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


class Enemy : public engine::IScript
{
public:
    Enemy(engine::IScene* my_scene, const engine_model_node_desc_t& node, const ModelInfo& model_info)
        : engine::IScript(my_scene)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

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
        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.75f;
        tc.scale[2] = 0.5f;

        tc.position[0] += 1.0f;
        tc.position[1] += 2.5f;
        tc.position[2] += 0.0f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        if (node.geometry_index != -1)
        {
            auto mc = engineSceneAddMeshComponent(scene, go_);
            mc.geometry = node.geometry_index != -1 ? model_info.geometries[node.geometry_index] : ENGINE_INVALID_OBJECT_HANDLE;
            engineSceneUpdateMeshComponent(scene, go_, &mc);
        }

        // if mesh is present then definitly we need some material to render it
        if (node.material_index != -1)
        {
            auto material_comp = engineSceneAddMaterialComponent(scene, go_);
            material_comp.material = model_info.materials.at(node.material_index);
            engineSceneUpdateMaterialComponent(scene, go_, &material_comp);
        }

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


class Sword : public engine::IScript
{
public:
    Sword(engine::IScene* my_scene, const engine_model_node_desc_t& node, const ModelInfo& model_info)
        : IScript(my_scene)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

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
        tc.scale[0] = 0.05f;
        tc.scale[1] = 0.40f;
        tc.scale[2] = 0.05f;

        tc.position[0] += 0.0f;
        tc.position[1] += 0.20f;
        tc.position[2] += 0.0f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        if (node.geometry_index != -1)
        {
            auto mc = engineSceneAddMeshComponent(scene, go_);
            mc.geometry = node.geometry_index != -1 ? model_info.geometries[node.geometry_index] : ENGINE_INVALID_OBJECT_HANDLE;
            engineSceneUpdateMeshComponent(scene, go_, &mc);
        }

        if (node.material_index != -1)
        {
            auto material_comp = engineSceneAddMaterialComponent(scene, go_);
            material_comp.material = model_info.materials.at(node.material_index);
            engineSceneUpdateMaterialComponent(scene, go_, &material_comp);
        }

        // physcis
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_BOX;
        set_c_array(cc.collider.box.size, std::array<float, 3>{ 0.15f, 0.15f, 0.15f});
        cc.is_trigger = true;
        engineSceneUpdateColliderComponent(scene, go_, &cc);

        // parent to hand
        engine_component_view_t cv{};
        engineCreateComponentView(&cv);
        engineSceneComponentViewAttachNameComponent(scene, cv);

        engine_component_iterator_t begin{};
        engine_component_iterator_t end{};
        engineComponentViewCreateBeginComponentIterator(cv, &begin);
        engineComponentViewCreateEndComponentIterator(cv, &end);

        while (!engineComponentIteratorCheckEqual(begin, end))
        {       
            auto go_it = engineComponentIteratorGetGameObject(begin);
            const auto nc = engineSceneGetNameComponent(scene, go_it);

            if (std::strcmp(nc.name, "Skeleton_arm_joint_R__3_") == 0)
            {
                auto pc = engineSceneAddParentComponent(scene, go_);
                pc.parent = go_it;
                engineSceneUpdateParentComponent(scene, go_, &pc);
                begin = end;
            }
            else
            {
                engineComponentIteratorNext(begin);
            }
        }
        engineDestroyComponentView(cv);
    }
};

class AnimationController
{
public:
    AnimationController(engine::IScene* my_scene, const std::vector<engine_game_object_t>& gos)
        : my_scene_(my_scene)
        , gos_(gos)
    {
    }

    void play_animation()
    {
        if (playin_animation_)
        {
            return;
        }
        playin_animation_ = true;
        animation_clip_index_ = 0;
        const auto scene = my_scene_->get_handle();
        for (const auto& go : gos_)
        {
            if (engineSceneHasAnimationClipComponent(scene, go))
            {
                auto anim_comp = engineSceneGetAnimationClipComponent(scene, go);
                auto& animation_clip = anim_comp.clips_array[animation_clip_index_];
                animation_clip.animation_dt = 0.0f;
                engineSceneUpdateAnimationClipComponent(scene, go, &anim_comp);
            }
        }
    }

    void update(float dt)
    {
        const auto scene = my_scene_->get_handle();
        if (playin_animation_)
        {
            for (const auto& go : gos_)
            {
                if (engineSceneHasAnimationClipComponent(scene, go))
                {
                    auto anim_comp = engineSceneGetAnimationClipComponent(scene, go);
                    //ToDo: duration can be pre-computed
                    auto& animation_clip = anim_comp.clips_array[animation_clip_index_];
                    const auto duration = animation_clip.channel_rotation.timestamps[animation_clip.channel_rotation.timestamps_count - 1];
                    animation_clip.animation_dt += dt;
                    engineSceneUpdateAnimationClipComponent(scene, go, &anim_comp);
                    if (animation_clip.animation_dt > duration)
                    {
                        playin_animation_ = false;
                    }
                }
            }
        }
    }

private:
    bool playin_animation_ = false;
    std::uint32_t animation_clip_index_ = 0;

    engine::IScene* my_scene_;
    std::vector<engine_game_object_t> gos_;
};

inline AnimationController* anim_controller_poc = nullptr;

class ControllableEntity : public engine::IScript
{
public:
    ControllableEntity(engine::IScene* my_scene, engine_game_object_t go)
        : IScript(my_scene, go)
    {
        const auto scene = my_scene->get_handle();
        const auto app = my_scene_->get_app_handle();
        // ------------ transform
        auto tc = engineSceneGetTransformComponent(scene, go_);
        auto quat_rot90y = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        const auto quat = quat_rot90y * glm::make_quat(tc.rotation);
        for (int i = 0; i < quat.length(); i++)
        {
            //tc.rotation[i] = quat[i];
        }
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // ------------ physcis
        // collider
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
        cc.collider.compound.children[0].type = ENGINE_COLLIDER_TYPE_BOX;
        const auto position_limits = engineApplicationGeometryGetAttributeLimits(app, engineApplicationGetGeometryByName(app, "unnamed_geometry__0"), ENGINE_VERTEX_ATTRIBUTE_TYPE_POSITION);
        std::array<float, 3> aabb_center{};
        std::array<float, 3> aabb_half_extent{};
        for (auto i = 0; i < aabb_half_extent.size(); i++)
        {
            aabb_center[i] = (position_limits.min[i] + position_limits.max[i]) / 2.0f;
            aabb_half_extent[i] = (std::abs(position_limits.min[i]) + std::abs(position_limits.max[i])) / 2.0f;
        }
        set_c_array(cc.collider.compound.children[0].rotation_quaternion, std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
        //set_c_array(cc.collider.compound.children[0].rotation_quaternion, std::array<float, 4>{quat.x, quat.y, quat.z, quat.w});
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

        if(anim_controller_poc)
        {
            if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_F))
            {
                anim_controller_poc->play_animation();
            }
            anim_controller_poc->update(dt);
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


inline bool load_controllable_mesh(engine_application_t& app, engine::IScene* scene_cpp)
{
    auto scene = scene_cpp->get_handle();
    engine_result_code_t engine_error_code = ENGINE_RESULT_CODE_FAIL;
    project_c::ModelInfo model_info(engine_error_code, app, "CesiumMan.gltf");

    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        return false;
    }

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
            scene_cpp->register_script<ControllableEntity>(go);
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
        if (node.skin_index != -1)
        {
            const auto& bones_game_object_arr = skin_to_game_object[node.skin_index];
            auto sc = engineSceneAddSkinComponent(scene, go);
            for (auto bone_idx = 0; bone_idx < bones_game_object_arr.size(); bone_idx++)
            {
                sc.bones[bone_idx] = bones_game_object_arr.at(bone_idx);
            }
            engineSceneUpdateSkinComponent(scene, go, &sc);
            log(fmt::format("Entity: {} added skin component for skin index: \n", go, node.skin_index));
        }
    }

    // animations
    auto copy_anim_channel_data = [](engine_animation_channel_t& out_channel, const engine_animation_channel_data_t& in_channel)
        {
            //timestamps
            out_channel.timestamps_count = in_channel.timestamps_count;
            assert(out_channel.timestamps_count < ENGINE_ANIMATION_CHANNEL_MAX_DATA_SIZE);
            std::memcpy(out_channel.timestamps, in_channel.timestamps, in_channel.timestamps_count * sizeof(in_channel.timestamps[0]));

            // data
            out_channel.data_count = in_channel.data_count;
            assert(out_channel.data_count < ENGINE_ANIMATION_CHANNEL_MAX_DATA_SIZE);
            std::memcpy(out_channel.data, in_channel.data, in_channel.data_count * sizeof(in_channel.data[0]));
        };

    std::vector<engine_game_object_t> animated_go;
    for (auto anim_idx = 0; anim_idx < model_info.model_info.animations_counts; anim_idx++)
    {
        const auto& anim = model_info.model_info.animations_array[anim_idx];
        log(fmt::format("Adding animation: {}\n", anim.name));
        for (auto channel_idx = 0; channel_idx < anim.channels_count; channel_idx++)
        {
            const auto& channel = anim.channels[channel_idx];
            const auto& go = node_id_to_game_object[channel.model_node_index];
            animated_go.push_back(go);
            auto anim = engineSceneAddAnimationClipComponent(scene, go);
            anim.clips_array[anim_idx].animation_dt = 0.0f;
            copy_anim_channel_data(anim.clips_array[anim_idx].channel_translation, channel.channel_translation);
            copy_anim_channel_data(anim.clips_array[anim_idx].channel_rotation, channel.channel_rotation);
            copy_anim_channel_data(anim.clips_array[anim_idx].channel_scale, channel.channel_scale);
            engineSceneUpdateAnimationClipComponent(scene, go, &anim);
        }
    }
    anim_controller_poc = new AnimationController(scene_cpp, animated_go);
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
    scene->register_script<project_c::Sword>(node, model_info);
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