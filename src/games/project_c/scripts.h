#include "animation_controller.h"
#include <iscript.h>
#include <iscene.h>
#include <engine.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

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
        engineSceneUpdateTransformComponent(scene, go_, &tc);
        // physcis
        //auto cc = engineSceneAddColliderComponent(scene, go_);
        //cc.type = ENGINE_COLLIDER_TYPE_BOX;
        //set_c_array(cc.collider.box.size, std::array<float, 3>{ 0.05f, 0.40f, 0.005f});
        //cc.is_trigger = true;
        //engineSceneUpdateColliderComponent(scene, go_, &cc);

         // physcis
        //auto cc = engineSceneAddColliderComponent(scene, go_);
        //cc.type = ENGINE_COLLIDER_TYPE_BOX;
        //set_c_array(cc.collider.box.size, std::array<float, 3>{ 0.1f, 0.1f, 0.1f });
        //engineSceneUpdateColliderComponent(scene, go_, &cc);

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
        tc.position[1] += 2.75f;
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
        auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
        rbc.mass = 1.0f;
        engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);
    }
};

class Solider : public BaseNode
{
public:
    Solider(engine::IScene* my_scene, engine_game_object_t go)
        : BaseNode(my_scene, go)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();
        set_name(scene, go_, "solider");
    }

    void update(float dt)
    {
        const auto scene = my_scene_->get_handle();
        const auto app = my_scene_->get_app_handle();

        const auto enemies = get_game_objects_with_name(scene, "enemy");
        for (const auto& e : enemies)
        {
            auto tc = engineSceneGetTransformComponent(scene, go_);
            auto ec = engineSceneGetTransformComponent(scene, e);
            // move to enemy
            //const auto dir = glm::normalize(glm::vec3(ec.position[0], ec.position[1], ec.position[2]) - glm::vec3(tc.position[0], tc.position[1], tc.position[2]));
            //tc.position[0] += dir.x * 0.0001f * dt;
            //tc.position[2] += dir.z * 0.0001f * dt;
            //engineSceneUpdateTransformComponent(scene, go_, &tc);

            // rotate toward enemy
            auto quat = rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
            
            quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.01f * dt);
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }

        auto tc = engineSceneGetTransformComponent(scene, go_);
        anim_controller_.set_active_animation("static");
        //anim_controller_.set_active_animation("crouch");
        //anim_controller_.set_active_animation("idle");


        const float speed = 0.0005f * dt;
        //std::string move_anim = "sprint";// "walk";
        std::string move_anim = "walk";

        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_W))
        {
            anim_controller_.set_active_animation(move_anim);
            tc.position[0] += speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_S))
        {
            anim_controller_.set_active_animation(move_anim);
            tc.position[0] -= speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_A))
        {
            anim_controller_.set_active_animation(move_anim);
            tc.position[2] -= speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_D))
        {
            anim_controller_.set_active_animation(move_anim);
            tc.position[2] += speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }


        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_N))
        {
            anim_controller_.set_active_animation("attack-melee-right");
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_M))
        {
            anim_controller_.set_active_animation("attack-melee-left");
        }
        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_G))
        {
            anim_controller_.set_active_animation("die");
        }
        anim_controller_.update(dt);
    }
};

}