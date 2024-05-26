#include "enemy_script.h"
#include "scripts_utils.h"

#include "iscene.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>


project_c::Enemy::Enemy(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z)
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

project_c::Enemy::~Enemy()
{
    utils::delete_game_objects_hierarchy(my_scene_->get_handle(), go_);
}

void project_c::Enemy::update(float dt)
{
    //anim_controller_.update(dt);
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    const auto player = utils::get_game_objects_with_name(scene, "solider")[0];
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
            //anim_controller_.set_active_animation("die");
        }
        else
        {
            if (distance_to_player < 0.8f)
            {
                state_ = States::ATTACK;
                //anim_controller_.set_active_animation(attack_data_.get_animation_name());
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
        //anim_controller_.set_active_animation("idle");
        state_ = States::DECISION_MAKE;
    }
    case States::ATTACK:
    {
        //if (!anim_controller_.is_active_animation(attack_data_.get_animation_name()))
        if (true)
        {
            state_ = States::DECISION_MAKE;
            attack_data_.attack_with_right = !attack_data_.attack_with_right;
        }
        break;
    }
    case States::DIE:
    {
        //if (!anim_controller_.is_active_animation("die"))
        if (true)
        {
            my_scene_->unregister_script(this);
        }
        break;
    }
    case States::MOVE:
    {
        //anim_controller_.set_active_animation("walk");

        auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
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