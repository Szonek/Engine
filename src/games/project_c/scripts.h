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

class BaseNode : public engine::IScript
{
public:
    AnimationController& get_animation_controller() { return anim_controller_; }

protected:
    BaseNode(engine::IScene* my_scene, engine_game_object_t go)
        : engine::IScript(my_scene, go)
    {

    }

protected:
    AnimationController anim_controller_;
};

class Floor : public BaseNode
{
public:
    Floor(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        set_name(scene, go_, "floor");
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
        set_c_array(mat_cd.diffuse_color, std::array<float, 4>({ 0.23, 0.7, 0.44f, 1.0f }));
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

class Sword : public BaseNode
{
public:
    Sword(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go)
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


    void on_collision(const collision_t& info)
    {
        engineLog(fmt::format("hit: {}\n", info.other).c_str());
    }
};

class Enemy : public BaseNode
{
public:
    Enemy(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        set_name(scene, go_, "enemy");
        auto tc = engineSceneGetTransformComponent(scene, go_);

        tc.position[0] += 1.0f;
        tc.position[1] -= 0.25f;
        tc.position[2] += 0.0f;
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
        //auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
        //rbc.mass = 1.0f;
        //engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
    }

    void update(float dt)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        {
            anim_controller_.set_active_animation("idle");
            anim_controller_.update(dt);
        }
        {
            const auto player = get_game_objects_with_name(scene, "solider")[0];
            auto tc = engineSceneGetTransformComponent(scene, go_);
            auto ec = engineSceneGetTransformComponent(scene, player);
            // rotate toward enemy
            auto quat = rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
            // use slerp to interpolate between current rotation and target rotation
            quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
    }
};

class Solider : public BaseNode
{
public:
    Solider(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go)
        , target_move_hit_({ENGINE_INVALID_GAME_OBJECT_ID})
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        set_name(scene, go_, "solider");

        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.position[0] = -0.25f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);
    }

    void update(float dt)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        const float speed_cooef = 0.005f;
        const float speed = speed_cooef * dt;

        auto tc = engineSceneGetTransformComponent(scene, go_);
        anim_controller_.set_active_animation("static");


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
                }
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

                // rotate
                auto quat = rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(target_move_hit_.position[0], target_move_hit_.position[1], target_move_hit_.position[2]));
                std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));

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
        anim_controller_.update(dt);
    }

    private:
        engine_ray_hit_info_t target_move_hit_{};
};

}