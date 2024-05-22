#include "animation_controller.h"
#include <iscript.h>
#include <iscene.h>
#include <engine.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <fmt/format.h>
#include <random>
#include <array>
#include <chrono>

namespace
{

inline void delete_game_objects_hierarchy(engine_scene_t scene, engine_game_object_t go)
{
    if (engineSceneHasChildrenComponent(scene, go))
    {
        auto cc = engineSceneGetChildrenComponent(scene, go);
        for (auto i = 0; i < std::size(cc.child); i++)
        {
            if (cc.child[i] != ENGINE_INVALID_GAME_OBJECT_ID)
            {
                delete_game_objects_hierarchy(scene, cc.child[i]);
                engineSceneDestroyGameObject(scene, cc.child[i]);
            }
        }
    }
}

inline void set_name(engine_scene_t scene, engine_game_object_t go, const char* name)
{
    engine_name_component_t nc{};
    if (!engineSceneHasNameComponent(scene, go))
    {
        engineSceneAddNameComponent(scene, go);
    }
    std::strncpy(nc.name, name, strlen(name));
    engineSceneUpdateNameComponent(scene, go, &nc);
}

inline std::vector<engine_game_object_t> get_active_camera_game_objects(engine_scene_t scene)
{
    engine_component_view_t cv{};
    engineCreateComponentView(&cv);
    engineSceneComponentViewAttachCameraComponent(scene, cv);

    engine_component_iterator_t begin{};
    engine_component_iterator_t end{};
    engineComponentViewCreateBeginComponentIterator(cv, &begin);
    engineComponentViewCreateEndComponentIterator(cv, &end);

    std::vector<engine_game_object_t> ret{};
    while (!engineComponentIteratorCheckEqual(begin, end))
    {
        auto go_it = engineComponentIteratorGetGameObject(begin);
        if (engineSceneHasCameraComponent(scene, go_it))
        {
            if (engineSceneGetCameraComponent(scene, go_it).enabled)
            {
                ret.push_back(go_it);
            }
        }
        engineComponentIteratorNext(begin);
    }
    engineDestroyComponentView(cv);
    return ret;
}

inline std::vector<engine_game_object_t> get_game_objects_with_name(engine_scene_t scene, std::string_view name)
{
    engine_component_view_t cv{};
    engineCreateComponentView(&cv);
    engineSceneComponentViewAttachNameComponent(scene, cv);

    engine_component_iterator_t begin{};
    engine_component_iterator_t end{};
    engineComponentViewCreateBeginComponentIterator(cv, &begin);
    engineComponentViewCreateEndComponentIterator(cv, &end);

    std::vector<engine_game_object_t> ret{};
    while (!engineComponentIteratorCheckEqual(begin, end))
    {
        auto go_it = engineComponentIteratorGetGameObject(begin);
        if (engineSceneHasNameComponent(scene, go_it))
        {
            if (0 == std::strcmp(engineSceneGetNameComponent(scene, go_it).name, name.data()))
            {
                ret.push_back(go_it);
            }
        }
        engineComponentIteratorNext(begin);
    }
    engineDestroyComponentView(cv);
    return ret;
}

inline glm::quat rotate_toward(glm::vec3 origin, glm::vec3 target)
{
    const auto dir = glm::normalize(target - origin);
    const auto angle = glm::degrees(std::atan2(dir.x, dir.z));
    return glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
}

inline engine_ray_t get_ray_from_mouse_position(engine_application_t app, engine_scene_t scene, engine_game_object_t go_camera)
{
    engine_ray_t ray{};
    // ray origin
    const auto camera_transform = engineSceneGetTransformComponent(scene, go_camera);
    ray.origin[0] = camera_transform.position[0];
    ray.origin[1] = camera_transform.position[1];
    ray.origin[2] = camera_transform.position[2];

    //unproject mouse coords to 3d space to get direction
    glm::vec4 viewport(0.0f, 0.0f, 1140.0f, 540.0f);
    const auto camera = engineSceneGetCameraComponent(scene, go_camera);
    const auto mouse_coords = engineApplicationGetMouseCoords(app);
    const auto mouse_x = mouse_coords.x * viewport.z;
    const auto mouse_y = mouse_coords.y * viewport.w;

    glm::mat4 view = glm::mat4(0.0);
    glm::mat4 projection = glm::mat4(0.0);
    // update camera: view and projection
    {
        const auto z_near = camera.clip_plane_near;
        const auto z_far = camera.clip_plane_far;
        // ToD: multi camera - this should use resolution of camera!!!

        const auto adjusted_width = viewport.z * (camera.viewport_rect.width - camera.viewport_rect.x);
        const auto adjusted_height = viewport.w * (camera.viewport_rect.height - camera.viewport_rect.y);
        const float aspect = adjusted_width / adjusted_height;

        if (camera.type == ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC)
        {
            const float scale = camera.type_union.orthographics_scale;
            projection = glm::ortho(-aspect * scale, aspect * scale, -scale, scale, z_near, z_far);
        }
        else
        {
            projection = glm::perspective(glm::radians(camera.type_union.perspective_fov), aspect, z_near, z_far);
        }
        const auto eye_position = glm::make_vec3(camera_transform.position);
        const auto up = glm::make_vec3(camera.direction.up);
        const auto target = glm::make_vec3(camera.target);
        view = glm::lookAt(eye_position, target, up);
    }

    const auto ray_dir = glm::unProject(glm::vec3(mouse_x, mouse_y, 1.0f), view, projection, viewport);
    ray.direction[0] = ray_dir.x;
    ray.direction[1] = ray_dir.y;
    ray.direction[2] = ray_dir.z;
    return ray;
}

}

namespace project_c
{
class CameraScript : public engine::IScript
{
public:
    CameraScript(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        auto nc = engineSceneAddNameComponent(scene, go_);
        std::strncpy(nc.name, "camera", strlen("camera"));
        engineSceneUpdateNameComponent(scene, go_, &nc);

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
        camera_transform_comp.position[0] = -2.0f;
        camera_transform_comp.position[1] = 4.0f;
        camera_transform_comp.position[2] = 1.0f;
        engineSceneUpdateTransformComponent(scene, go_, &camera_transform_comp);
    }

    void update(float dt) override
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        const auto mouse_coords = engineApplicationGetMouseCoords(app);

        const float move_speed = 1.0f * dt;
        const auto character_go = get_game_objects_with_name(scene, "solider")[0];
        // follow character go
        if (character_go != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            //first update position
            const auto character_tc = engineSceneGetTransformComponent(scene, character_go);
            auto tc = engineSceneGetTransformComponent(scene, go_);
            tc.position[0] = character_tc.position[0] - 2.0f;
            tc.position[1] = character_tc.position[1] + 7.0f;
            tc.position[2] = character_tc.position[2] + 2.5f;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
            // update targer to point to character go position
            auto camera_comp = engineSceneGetCameraComponent(scene, go_);
            camera_comp.target[0] = character_tc.position[0];
            camera_comp.target[1] = character_tc.position[1];
            camera_comp.target[2] = character_tc.position[2];
            engineSceneUpdateCameraComponent(scene, go_, &camera_comp);
        }
    }
};

class BaseNode : public engine::IScript
{
public:
    AnimationController& get_animation_controller() { return anim_controller_; }

protected:
    BaseNode(engine::IScene* my_scene, std::string_view name)
        : BaseNode(my_scene, engineSceneCreateGameObject(my_scene->get_handle()), name)
    {
    }

    BaseNode(engine::IScene* my_scene, engine_game_object_t go, std::string_view name)
        : engine::IScript(my_scene, go)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        set_name(scene, go_, name.data());
    }

protected:
    AnimationController anim_controller_;
};

class SpecularBox : public BaseNode
{
public:
    SpecularBox(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_y, float offset_z)
        : BaseNode(my_scene, go, "specular-box")
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        // transform
        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.position[0] += offset_x;
        tc.position[1] += offset_y;
        tc.position[2] += offset_z;
        
        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.5f;
        tc.scale[2] = 0.5f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // material
        auto mc = engineSceneGetMaterialComponent(scene, go_);
        mc.material = engineApplicationGetMaterialByName(app, "cube_material");
        engineSceneUpdateMaterialComponent(scene, go_, &mc);

    }

};

class MainLight : public BaseNode
{
public:
    MainLight(engine::IScene* my_scene)
        : BaseNode(my_scene, "main-light")
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        // position in world
        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = 0.0f;
        tc.position[1] = 10.0f;
        tc.position[2] = 0.0f;

        tc.scale[0] = 0.1f;
        tc.scale[1] = 0.1f;
        tc.scale[2] = 0.1f;

        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // for visulastuion add mesh component
        auto mc = engineSceneAddMeshComponent(scene, go_);
        mc.geometry = engineApplicationGetGeometryByName(my_scene_->get_app_handle(), "cube.glb");
        engineSceneUpdateMeshComponent(scene, go_, &mc);

        // and basic material
        auto mat = engineSceneAddMaterialComponent(scene, go_);
        engineSceneUpdateMaterialComponent(scene, go_, &mat);

        // light component
        auto lc = engineSceneAddLightComponent(scene, go_);
        lc.type = ENGINE_LIGHT_TYPE_DIRECTIONAL;
        set_c_array(lc.intensity.ambient, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
        set_c_array(lc.intensity.diffuse, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
        set_c_array(lc.intensity.specular, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
        set_c_array(lc.directional.direction, std::array<float, 3>{ 0.0f, 1.0f, 0.0f });
        engineSceneUpdateLightComponent(scene, go_, &lc);
    }

};

class Floor : public BaseNode
{
public:
    Floor(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
        : BaseNode(my_scene, go, "floor")
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.position[0] += offset_x;
        tc.position[2] += offset_z;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // physcis
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_BOX;
        set_c_array(cc.collider.box.size, std::array<float, 3>{ 0.5f, 0.01f, 0.5f });
        engineSceneUpdateColliderComponent(scene, go_, &cc);
    }
};

class Wall : public BaseNode
{
public:
    Wall(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
        : BaseNode(my_scene, go, "wall")
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.position[0] += offset_x;
        tc.position[2] += offset_z;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // physcis
        //auto cc = engineSceneAddColliderComponent(scene, go_);
        //cc.type = ENGINE_COLLIDER_TYPE_BOX;
        //set_c_array(cc.collider.box.size, std::array<float, 3>{ 0.5f, 0.01f, 0.5f });
        //engineSceneUpdateColliderComponent(scene, go_, &cc);
    }
};

class Barrel : public BaseNode
{
public:
    Barrel(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go, "barrel")
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.position[1] -= 0.15f;
        tc.position[2] += 1.0f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // physcis
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
        auto& child_c = cc.collider.compound.children[0];
        {
            child_c.type = ENGINE_COLLIDER_TYPE_BOX;
            child_c.transform[1] = 0.2f;
            child_c.rotation_quaternion[3] = 1.0f;
            set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.2f, 0.2f, 0.2f});
        }
        engineSceneUpdateColliderComponent(scene, go_, &cc);
    }
};


class Enemy : public BaseNode
{
private:
    enum class States
    {
        DECISION_MAKE = 0,
        IDLE = 1,
        ATTACK,
        DIE,
        MOVE
    };

    struct IdleStateData {};
    struct AttackStateData
    {
        bool attack_with_right = false;
        inline const char* get_animation_name() const
        {
            return attack_with_right ? "attack-melee-right" : "attack-melee-left";
        }
    };
    struct DyingStateData{};
    struct MoveStateData{};

public:
    std::int32_t hp = 20;
    Enemy(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
        : BaseNode(my_scene, go, "enemy")
        , state_(States::DECISION_MAKE)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        auto tc = engineSceneGetTransformComponent(scene, go_);

        tc.position[0] += 1.0f + offset_x;
        tc.position[1] -= 0.25f;
        //tc.position[1] += 1.25f;
        tc.position[2] += 0.0f + offset_z;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // physcis
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
        auto& child_c = cc.collider.compound.children[0];
        {
            child_c.type = ENGINE_COLLIDER_TYPE_BOX;
            child_c.transform[0] = 0.0f;
            child_c.transform[1] = 0.35f;
            child_c.transform[2] = 0.0f;
            child_c.rotation_quaternion[3] = 1.0f;
            set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.3f, 0.35f, 0.3f});
        }
        engineSceneUpdateColliderComponent(scene, go_, &cc);

        //rb
        auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
        rbc.mass = 1.0f;
        //rbc.mass = 0.0f;
        engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
    }

    virtual ~Enemy()
    {
        delete_game_objects_hierarchy(my_scene_->get_handle(), go_);
    }

    void update(float dt)
    {
        anim_controller_.update(dt);
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        const auto player = get_game_objects_with_name(scene, "solider")[0];
        auto tc = engineSceneGetTransformComponent(scene, go_);
        auto ec = engineSceneGetTransformComponent(scene, player);
        const auto distance_to_player = glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(ec.position[0], ec.position[2]));

        switch (state_)
        {
        case States::DECISION_MAKE:
        {
            if (hp <= 0)
            {
                state_ = States::DIE;
                anim_controller_.set_active_animation("die");
            }
            else
            {
                if (distance_to_player < 0.8f)
                {
                    state_ = States::ATTACK;
                    anim_controller_.set_active_animation(attack_data_.get_animation_name());
                }
                else if (distance_to_player < 3.0f)
                {
                    state_ = States::MOVE;
                }
                else
                {
                    state_ = States::IDLE;
                }
            }
            break;
        }
        case States::IDLE:
        {
            anim_controller_.set_active_animation("idle");
            state_ = States::DECISION_MAKE;
        }
        case States::ATTACK:
        {
            if (!anim_controller_.is_active_animation(attack_data_.get_animation_name()))
            {
                state_ = States::DECISION_MAKE;
                attack_data_.attack_with_right = !attack_data_.attack_with_right;
            }        
            break;
        }
        case States::DIE:
        {
            if (!anim_controller_.is_active_animation("die"))
            {
                my_scene_->unregister_script(this);
            }         
            break;
        }
        case States::MOVE:
        {
            anim_controller_.set_active_animation("walk");

            auto quat = rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
            quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            const float speed_cooef = 0.001f;
            const float speed = speed_cooef * dt;
            const glm::vec3 forward = glm::normalize(quat * glm::vec3(0.0f, 0.0f, 1.0f));
            tc.position[0] += forward.x * speed;
            //tc.position[1] += forward.y * speed;
            tc.position[2] += forward.z * speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
            state_ = States::DECISION_MAKE;
            break;
        }
        default:
        {
            engineLog("Unknown enemy state\n");
            break;
        }
        }
    }

private:
    bool triggered_ = false;
    bool attack_right_ = false;
    States state_;
    AttackStateData attack_data_;

};

class Sword : public BaseNode
{
public:
    Sword(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go, "weapon-sword")
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.position[0] = -0.2f;
        tc.position[1] = 0.0f;
        tc.position[2] = 0.1f;

        auto rotation = glm::angleAxis(glm::radians(-65.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        std::memcpy(tc.rotation, glm::value_ptr(rotation), sizeof(tc.rotation));
        engineSceneUpdateTransformComponent(scene, go_, &tc);
        
        
        // physcis
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
        cc.is_trigger = true;
        auto& cc_child = cc.collider.compound.children[0];
        cc_child.rotation_quaternion[3] = 1.0f;
        cc_child.transform[1] -= 0.2f;
        cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
        set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.05f, 0.20f, 0.005f});
        engineSceneUpdateColliderComponent(scene, go_, &cc);

        // parent to hand
        const auto parent = get_game_objects_with_name(scene, "arm-right")[0];
        if (parent != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            auto pc = engineSceneAddParentComponent(scene, go_);
            pc.parent = parent;
            engineSceneUpdateParentComponent(scene, go_, &pc);
        }
    }
};

class AttackTrigger : public BaseNode
{
public:
    AttackTrigger(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go, "attack-trigger")
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        // transform
        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = 0.0f;
        tc.position[1] = 0.0f;
        tc.position[2] = 0.0f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // physcis
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
        cc.is_trigger = true;
        auto& cc_child = cc.collider.compound.children[0];
        cc_child.rotation_quaternion[3] = 1.0f;
        cc_child.transform[1] = 0.21f;
        cc_child.transform[2] = 0.6f;
        cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
        set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.3f, 0.05f, 0.3f});
        engineSceneUpdateColliderComponent(scene, go_, &cc);

        // parent to root
        const auto gos_with_root_name = get_game_objects_with_name(scene, "solider");
        for (auto& parent : gos_with_root_name)
        {
            if (parent != ENGINE_INVALID_GAME_OBJECT_ID)
            {
                auto pc = engineSceneAddParentComponent(scene, go_);
                pc.parent = parent;
                engineSceneUpdateParentComponent(scene, go_, &pc);
                break;
            }
        }
    }

    void activate()
    {
        is_active_ = true;
    }

    void on_collision_enter(const collision_t& info) override
    {
        if (is_active_)
        {
            if (auto* enemy = my_scene_->get_script<Enemy>(info.other))
            {
                enemy->hp -= 10;
            }
        }
    }

    void update(float dt) override
    {
        // deactivate trigger after one frame?
        if (is_active_)
        {
            is_active_ = false;
        }
    }

private:
    bool is_active_ = false;
};

class Solider : public BaseNode
{
private:
    enum class States
    {
        IDLE = 0,
        ATTACK,
        MOVE,
        DODGE,
    };

    struct GlobalStateData
    {
        engine_ray_hit_info_t last_mouse_hit = {};
    };

    struct DodgeStateData
    {
        std::chrono::milliseconds dodge_timer_cooldown = std::chrono::milliseconds(0);
        std::chrono::milliseconds dodge_timer_animation = std::chrono::milliseconds(0);
        
        void update(float dt)
        {
            if (animation_playing_)
            {
                dodge_timer_animation += std::chrono::milliseconds(static_cast<std::int64_t>(dt));
            }
            if (cooldown_playing_)
            {
                dodge_timer_cooldown += std::chrono::milliseconds(static_cast<std::int64_t>(dt));
            }

            if (dodge_timer_animation >= std::chrono::milliseconds(150))
            {
                dodge_timer_animation = std::chrono::milliseconds(0);
                animation_playing_ = false;
            }
            if (dodge_timer_cooldown >= std::chrono::milliseconds(3000))
            {
                dodge_timer_cooldown = std::chrono::milliseconds(0);
                cooldown_playing_ = false;
            }
        }

        inline bool animation_is_playing() const
        {
            return animation_playing_;
        }

        inline void activate()
        {
            animation_playing_ = true;
            cooldown_playing_  = true;
        }

        inline bool can_dodge() const
        {
            return !cooldown_playing_;
        }
    private:
        bool animation_playing_ = false;
        bool cooldown_playing_ = false;
    };

    struct AttackStateData
    {
        bool animation_started = false;
        inline const char* get_animation_name() const
        {
            return "attack-melee-right";
        }
    };

public:
    Solider(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go, "solider")
        , attack_trigger_(nullptr)
        , state_(States::IDLE)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.position[1] = -0.25f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        // physcis
        auto cc = engineSceneAddColliderComponent(scene, go_);
        cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
        cc.is_trigger = false;
        auto& cc_child = cc.collider.compound.children[0];
        cc_child.rotation_quaternion[3] = 1.0f;
        cc_child.transform[1] = 0.45f;
        cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
        set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.3f, 0.35f, 0.2f});
        engineSceneUpdateColliderComponent(scene, go_, &cc);

        //rb
        auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
        rbc.mass = 100000.0f;
        engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);

        // add attack trigger
        attack_trigger_ = my_scene_->register_script<AttackTrigger>(engineSceneCreateGameObject(my_scene->get_handle()));
    }

    void update(float dt)
    {
        anim_controller_.update(dt);
        dodge_data_.update(dt);
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();


        auto rotate_towards_global_target = [&]()
        {
            auto tc = engineSceneGetTransformComponent(scene, go_);
            auto quat = rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(global_data_.last_mouse_hit.position[0], global_data_.last_mouse_hit.position[1], global_data_.last_mouse_hit.position[2]));
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        };

        const auto lmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT);
        const auto rmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_RIGHT);
        if ((lmb || rmb) && state_ != States::DODGE)
        {
            const auto ray = get_ray_from_mouse_position(app, scene, get_active_camera_game_objects(scene)[0]);
            const std::array<engine_game_object_t, 1> raycast_ignore_list = { attack_trigger_->get_game_object() };
            const auto hit_info = engineScenePhysicsRayCast(scene, raycast_ignore_list.data(), raycast_ignore_list.size(), &ray, 1000.0f);
            if (hit_info.go != ENGINE_INVALID_GAME_OBJECT_ID)
            {
                global_data_.last_mouse_hit = hit_info;
                state_ = lmb ? States::MOVE : States::ATTACK;
            }
        }
        
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_SPACE) && dodge_data_.can_dodge())
        {
            state_ = States::DODGE;
            dodge_data_.activate();
            const auto ray = get_ray_from_mouse_position(app, scene, get_active_camera_game_objects(scene)[0]);
        }

        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_1))
        {
            auto cc = engineSceneGetColliderComponent(scene, attack_trigger_->get_game_object());
            cc.collider.compound.children->collider.box.size[0] = 2.6f;
            cc.collider.compound.children->collider.box.size[2] = 2.6f;
            engineSceneUpdateColliderComponent(scene, attack_trigger_->get_game_object(), &cc);

        }
        else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_2))
        {
            auto cc = engineSceneGetColliderComponent(scene, attack_trigger_->get_game_object());
            cc.collider.compound.children->collider.box.size[0] = 0.3f;
            cc.collider.compound.children->collider.box.size[2] = 0.3f;
            engineSceneUpdateColliderComponent(scene, attack_trigger_->get_game_object(), &cc);
        }
        switch (state_)
        {
        case States::IDLE:
        {
            anim_controller_.set_active_animation("idle");
            break;
        }
        case States::DODGE:
        {
            if (!dodge_data_.animation_is_playing())
            {
                state_ = States::IDLE;
            }
            else
            {
                anim_controller_.set_active_animation("crouch");
                const float speed_cooef = 0.015f;
                const float speed = speed_cooef * dt;
                auto tc = engineSceneGetTransformComponent(scene, go_);
                // move
                const glm::quat rotation = glm::make_quat(tc.rotation); // Convert the rotation to a glm::quat
                const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f); // Get the forward direction vector
                const glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)); // Calculate the right direction vector
                tc.position[0] += forward.x * speed;
                //tc.position[1] += forward.y * speed;  // dont go up!
                tc.position[2] += forward.z * speed;
                engineSceneUpdateTransformComponent(scene, go_, &tc);
            }
            break;
        }
        case States::ATTACK:
        {
            if (attack_data_.animation_started)
            {
                if (!anim_controller_.is_active_animation(attack_data_.get_animation_name()))
                {
                    state_ = States::IDLE;
                    attack_data_ = {};
                }
            }
            else
            {
                rotate_towards_global_target();
                anim_controller_.set_active_animation(attack_data_.get_animation_name());
                attack_data_.animation_started = true;
                attack_trigger_->activate();
            }

            break;
        }
        case States::MOVE:
        {
            const float speed_cooef = 0.0025f;
            const float speed = speed_cooef * dt;

            auto tc = engineSceneGetTransformComponent(scene, go_);
            const auto distance = glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(global_data_.last_mouse_hit.position[0], global_data_.last_mouse_hit.position[2]));
            if (distance < 0.05f)
            {
                state_ = States::IDLE;
            }
            else
            {
                anim_controller_.set_active_animation("walk");

                rotate_towards_global_target();
                auto tc = engineSceneGetTransformComponent(scene, go_);
                const float speed_cooef = 0.0025f;
                const float speed = speed_cooef * dt;
                // move
                const glm::quat rotation = glm::make_quat(tc.rotation); // Convert the rotation to a glm::quat
                const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f); // Get the forward direction vector
                const glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)); // Calculate the right direction vector
                tc.position[0] += forward.x * speed;
                //tc.position[1] += forward.y * speed;  // dont go up!
                tc.position[2] += forward.z * speed;
                engineSceneUpdateTransformComponent(scene, go_, &tc);
            }

            break;
        }
        default:
        {
            engineLog("Unknown solider state\n");
            break;
        }
        }
    }

private:
    AttackTrigger* attack_trigger_;
    States state_;
    AttackStateData attack_data_;
    GlobalStateData global_data_;
    DodgeStateData dodge_data_;

};

}