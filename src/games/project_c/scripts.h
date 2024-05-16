#include "animation_controller.h"
#include <iscript.h>
#include <iscene.h>
#include <engine.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <fmt/format.h>

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
public:
    std::int32_t hp = 20;
    Enemy(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
        : BaseNode(my_scene, go, "enemy")
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
        if(anim_controller_.is_active_animation(attack_right_ ? "attack-melee-right" : "attack-melee-left"))
        {
            return;
        }
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        if (hp < 0)
        {
            my_scene_->unregister_script(this);
        }
        else
        {
            anim_controller_.set_active_animation("idle");

        }
        {
            const auto player = get_game_objects_with_name(scene, "solider")[0];
            auto tc = engineSceneGetTransformComponent(scene, go_);
            auto ec = engineSceneGetTransformComponent(scene, player);
            // rotate toward enemy

            // distance to player - if small enough move toward player
            const auto distance = glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(ec.position[0], ec.position[2]));
            if (distance <= 3.0f)
            {
                triggered_ = true;
            }
            else if (distance > 5.0f)
            {
                triggered_ = false;
            }
            if(triggered_)
            {

                auto quat = rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
                // use slerp to interpolate between current rotation and target rotation
                quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
                std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));

                //attack or move to player
                if (distance < 0.8f)
                {
                    attack_right_ = !attack_right_;
                    anim_controller_.set_active_animation(attack_right_ ? "attack-melee-right" : "attack-melee-left");
                }
                else
                {
                    const float speed_cooef = 0.0005f;
                    const float speed = speed_cooef * dt;
                    const glm::vec3 forward = glm::normalize(quat * glm::vec3(0.0f, 0.0f, 1.0f));
                    tc.position[0] += forward.x * speed;
                    //tc.position[1] += forward.y * speed;
                    tc.position[2] += forward.z * speed;
                }

                engineSceneUpdateTransformComponent(scene, go_, &tc);
            }
        }
    }

private:
    bool triggered_ = false;
    bool attack_right_ = false;
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

    //void on_collision_enter(const collision_t& info) override
    //{
    //    auto* enemy = my_scene_->get_script<Enemy>(info.other);
    //    if (active_ && enemy)
    //    {
    //       // engineLog(fmt::format("hit: {}\n", info.other).c_str());
    //        enemy->hp -= 10;
    //        active_ = false;
    //    }
    //}

    //void set_active(bool value) { active_ = value; }

//private:
//    bool active_ = false;
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

        // WA 
        // ToDo: fix me
        my_scene_->register_script(this);
    }

    void activate()
    {
        is_active_ = true;
    }

    void update(float dt) override
    {
        if (is_active_)
        {
            engineLog("attack trigger active\n");
            auto scene = my_scene_->get_handle();
            std::size_t collisions_count = 0;
            const engine_collision_info_t* collisions = nullptr;
            engineScenePhysicsGetCollisions(scene, &collisions_count, &collisions);
            std::vector<engine_game_object_t> gos_already_hit;
            for (auto i = 0; i < collisions_count; i++)
            {
                const auto obj_a = collisions[i].object_a;
                const auto obj_b = collisions[i].object_b;
                if (obj_a != go_ && obj_b != go_)
                {
                    //ToDo: we need API to expose collisions with concrete game object
                    continue;
                }
                auto* enemy_a = my_scene_->get_script<Enemy>(obj_a);
                auto* enemy_b = my_scene_->get_script<Enemy>(obj_b);
                Enemy* enemy = nullptr;
                if (enemy_a || enemy_b)
                {
                    //engineLog("enemy hit!\n");
                    enemy = enemy_a ? enemy_a : enemy_b;
                }
                if (enemy)
                {
                    //ToDo: WA hack to avoid multiple hits for the same enemy - physics shouldnt report diffeent collisions for 2 same objects?
                    if (std::find(gos_already_hit.begin(), gos_already_hit.end(), enemy->get_game_object()) != gos_already_hit.end())
                    {
                        continue;
                    }
                    gos_already_hit.push_back(enemy->get_game_object());
                    enemy->hp -= 10;
                }
            }

            is_active_ = false;
        }
    }

private:
    bool is_active_ = false;
};

class Solider : public BaseNode
{
public:
    Solider(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go, "solider")
        , target_move_hit_({ENGINE_INVALID_GAME_OBJECT_ID})
        , attack_trigger_(my_scene, engineSceneCreateGameObject(my_scene->get_handle()))
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

    }

    void update(float dt)
    {
        anim_controller_.update(dt);
        if (anim_controller_.is_active_animation("attack-melee-right"))
        {
            return;
        }

        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        Sword* sword_script = my_scene_->get_script<Sword>(get_game_objects_with_name(scene, "weapon-sword")[0]);
        //sword_script->set_active(false);

        const float speed_cooef = 0.0025f;
        const float speed = speed_cooef * dt;

        auto tc = engineSceneGetTransformComponent(scene, go_);
        anim_controller_.set_active_animation("idle");


        // raycast
        const auto lmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT);
        const auto rmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_RIGHT);
        if (lmb || rmb)
        {
            const auto ray = get_ray_from_mouse_position(app, scene, get_active_camera_game_objects(scene)[0]);
            const auto hit_info = engineScenePhysicsRayCast(scene, &ray, 1000.0f);
            if (ENGINE_INVALID_GAME_OBJECT_ID != hit_info.go)
            {
                const auto name = engineSceneGetNameComponent(scene, hit_info.go).name;
                const auto distance = glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(hit_info.position[0], hit_info.position[2]));
                if (distance < speed)
                {
                    target_move_hit_ = {};
                }
                else
                {
                    target_move_hit_ = hit_info;
                    // rotate
                    auto quat = rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(target_move_hit_.position[0], target_move_hit_.position[1], target_move_hit_.position[2]));
                    std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
                    engineSceneUpdateTransformComponent(scene, go_, &tc);
                }
            }

            if (rmb)
            {
                target_move_hit_ = {};
            }
        }



        if (target_move_hit_.go != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            const auto distance = glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(target_move_hit_.position[0], target_move_hit_.position[2]));
            if (distance < speed)
            {
                anim_controller_.set_active_animation("idle");
                target_move_hit_ = {};
            }
            else
            {
                anim_controller_.set_active_animation("walk");
                // helper math to move forward
                const glm::quat rotation = glm::make_quat(tc.rotation); // Convert the rotation to a glm::quat
                const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f); // Get the forward direction vector
                const glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)); // Calculate the right direction vector

                // move
                tc.position[0] += forward.x * speed;
                //tc.position[1] += forward.y * speed;  // dont go up!
                tc.position[2] += forward.z * speed;
                engineSceneUpdateTransformComponent(scene, go_, &tc);
            }

        }

        if (engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_RIGHT))
        {
            anim_controller_.set_active_animation("attack-melee-right");
            attack_trigger_.activate();
            //sword_script->set_active(true);
        }
        //if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_N))
        //{
        //    anim_controller_.set_active_animation("attack-melee-right");
        //}
        //if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_M))
        //{
        //    anim_controller_.set_active_animation("attack-melee-left");
        //}
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_G))
        {
            anim_controller_.set_active_animation("die");
        }
    }

    private:
        engine_ray_hit_info_t target_move_hit_{};
        AttackTrigger attack_trigger_;
};

}