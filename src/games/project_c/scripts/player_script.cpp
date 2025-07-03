#include "player_script.h"
#include "scripts_utils.h"
#include "enemy_script.h"
#include "enviorment_script.h"
#include "interactable_script.h"
#include "../scenes/scene_test.h"

#include "../app.h"
#include "iscene.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

project_c::Weapon::Weapon(engine::IScene* my_scene)
    : BaseNode(my_scene, "weapon-sword")
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    // transform
    auto tc = engineSceneAddTransformComponent(scene, go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // mesh
    auto mc = engineSceneAddMeshComponent(scene, go_);
    mc.geometry = engineApplicationGetGeometryByName(app, "Cube.12900");
    assert(mc.geometry != ENGINE_INVALID_OBJECT_HANDLE);
    engineSceneUpdateMeshComponent(scene, go_, &mc);

    // material
    auto matc = engineSceneAddMaterialComponent(scene, go_);
    set_c_array(matc.data.pong.diffuse_color, std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f});
    matc.data.pong.shininess = 32.0f;
    matc.data.pong.diffuse_texture = engineApplicationGetTextured2DByName(app, "barbarian_texture");
    engineSceneUpdateMaterialComponent(scene, go_, &matc);

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.bounciness = 0.35f;
    cc.friction_static = 0.9f;
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = true;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.transform[0] -= 0.05f;
    cc_child.transform[1] = 0.25f;
    cc_child.transform[2] = 0.0f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    // ToDo: box sie could be smaller and only increase box size when item dropped on the ground, so it do not fly trhoguh ground
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.1f, 0.10f, 0.04f});
    engineSceneUpdateColliderComponent(scene, go_, &cc);
}

project_c::Weapon::~Weapon()
{
}

void project_c::Weapon::attach_to_game_object(engine_game_object_t parent, std::string_view joint_name, std::optional<glm::vec3> position = std::nullopt, std::optional<glm::quat> rotation = std::nullopt)
{
    const auto scene = my_scene_->get_handle();
    // parent to hand
    if (parent != ENGINE_INVALID_GAME_OBJECT_ID)
    {
        auto pc = engineSceneAddParentComponent(scene, go_);
        pc.parent = parent;
        engineSceneUpdateParentComponent(scene, go_, &pc);
    }

    auto jac = engineSceneAddJointAttachmentComponent(scene, go_);
    jac.skin = engineSceneGetSkinComponent(scene, parent).skin;
    engineStringSet(jac.joint_name, joint_name.data());
    engineSceneUpdateJointAttachmentComponent(scene, go_, &jac);

    if (position.has_value() || rotation.has_value())
    {
        auto tc = engineSceneGetTransformComponent(scene, go_);

        if (position.has_value())
        {
            std::memcpy(tc.position, glm::value_ptr(position.value()), sizeof(tc.position));
        }
        if (rotation.has_value())
        {
            std::memcpy(tc.rotation, glm::value_ptr(rotation.value()), sizeof(tc.rotation));
        }     
        engineSceneUpdateTransformComponent(scene, go_, &tc);
    }
}

void project_c::Weapon::drop_on_ground(glm::vec3 position)
{
    const auto scene = my_scene_->get_handle();

    // set posion
    auto tc = engineSceneGetTransformComponent(scene, go_);
    tc.position[0] = position.x;
    tc.position[1] = position.y + 2.0f;
    tc.position[2] = position.z;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // remove parent
    if (engineSceneHasParentComponent(scene, go_))
    {
        engineSceneRemoveParentComponent(scene, go_);
    }

    // add rigid body component
    auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
    rbc.mass = 1000.0f;
    engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);

    // update collider to not be trigger, so it will stop on collision
    auto cc = engineSceneGetColliderComponent(scene, go_);
    cc.is_trigger = false;
    engineSceneUpdateColliderComponent(scene, go_, &cc);
}

void project_c::Weapon::on_collision(const collision_t& info)
{
    if (auto* floor = my_scene_->get_script<Floor>(info.other))
    {
        // remove rigid body and enalbe is trigger
        // check if has rigidbody (remove once), beacuse there can be multiple collision calls in single frame
        if (engineSceneHasRigidBodyComponent(my_scene_->get_handle(), go_))
        {
            engineSceneRemoveRigidBodyComponent(my_scene_->get_handle(), go_);
            auto cc = engineSceneGetColliderComponent(my_scene_->get_handle(), go_);
            cc.is_trigger = true;
            engineSceneUpdateColliderComponent(my_scene_->get_handle(), go_, &cc);
        }
    }
}

void project_c::Weapon::update(float dt)
{
    auto typed_scene = static_cast<project_c::TestScene*>(my_scene_);
    const auto scene = typed_scene->get_handle();

    if (!engineSceneHasParentComponent(scene, go_))
    {
        typed_scene->ui_update_item_on_ground(this);
    }
    else
    {
        typed_scene->ui_remove_item_from_ground(this);
    }

}


project_c::AttackTrigger::AttackTrigger(engine::IScene* my_scene, engine_game_object_t go)
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
    const auto gos_with_root_name = utils::get_game_objects_with_name(scene, "player");
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

void project_c::AttackTrigger::on_collision(const collision_t& info)
{
    if (is_active_)
    {
        if (auto* enemy = my_scene_->get_script<Enemy>(info.other))
        {
            enemy->hp -= 10;
        }
    }
}

void project_c::AttackTrigger::update(float dt)
{
    // deactivate trigger after one frame?
    if (is_active_)
    {
        is_active_ = false;
    }
}

void project_c::AttackTrigger::activate()
{
    is_active_ = true;
}

project_c::Player::Player(engine::IScene* my_scene, const PrefabResult& pr)
    : BaseNode(my_scene, pr, "player")
    , weapon_(nullptr)
    , state_(States::IDLE)
    , attack_trigger_(nullptr)
{
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    auto tc = engineSceneGetTransformComponent(scene, go_);
    tc.position[1] = -0.25f;
    tc.scale[0] = 0.35f;
    tc.scale[1] = 0.35f;
    tc.scale[2] = 0.35f;
    engineSceneUpdateTransformComponent(scene, go_, &tc);

    // physcis
    auto cc = engineSceneAddColliderComponent(scene, go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = false;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.transform[0] = 0.0f;
    cc_child.transform[1] = 1.0f;
    cc_child.transform[2] = 0.0f;
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.5f, 1.0f, 0.4f});
    engineSceneUpdateColliderComponent(scene, go_, &cc);

    //rb
    auto rbc = engineSceneAddRigidBodyComponent(scene, go_);
    rbc.mass = 100000.0f;
    engineSceneUpdateRigidBodyComponent(scene, go_, &rbc);

    // add handle to right arm
    right_arm_go_ = utils::get_game_objects_with_name(scene, "handslot.r")[0];
    assert(right_arm_go_ != ENGINE_INVALID_GAME_OBJECT_ID);
    // cleanup any childer of handslot (as model could be prebuilt with attached geomteries)
    if (engineSceneHasChildrenComponent(scene, right_arm_go_))
    {
        utils::delete_game_objects_hierarchy(scene, right_arm_go_);
    }
    left_arm_go_ = utils::get_game_objects_with_name(scene, "handslot.l")[0];
    assert(left_arm_go_ != ENGINE_INVALID_GAME_OBJECT_ID);
    // cleanup any childer of handslot (as model could be prebuilt with attached geomteries)
    if (engineSceneHasChildrenComponent(scene, left_arm_go_))
    {
        utils::delete_game_objects_hierarchy(scene, left_arm_go_);
    }

    // add attack trigger
    attack_trigger_ = my_scene_->register_script<AttackTrigger>(engineSceneCreateGameObject(scene));
}

void project_c::Player::update(float dt)
{


    auto check_state_bit = [&](States state)
        {
            return (state_ & state) != 0;
        };
    auto clear_state_bit = [&](States state)
        {
            state_ &= ~state;
        };
    auto enable_state_bit = [&](States state)
        {
            state_ |= state;
        };

    anim_controller_.update(dt);
    dodge_data_.update(dt);
    const auto scene = my_scene_->get_handle();
    const auto app = my_scene_->get_app_handle();

    if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_0))
    {
        set_world_position( 0.0f, 1.0f, 0.0f );
    }


    const std::array<engine_game_object_t, 1> raycast_ignore_list = { attack_trigger_->get_game_object() };
    const auto active_camera_go = utils::get_active_camera_game_objects(scene)[0];
    const auto ray = utils::get_ray_from_mouse_position(app, scene, active_camera_go);
    const auto hit_info = engineScenePhysicsRayCast(scene, raycast_ignore_list.data(), raycast_ignore_list.size(), &ray, 1000000.0f);

    auto rotate_towards_global_target = [&]()
        {
            auto tc = engineSceneGetTransformComponent(scene, go_);
            auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(hit_info.position.x, hit_info.position.y, hit_info.position.z));
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        };
    rotate_towards_global_target();// rotate towards target

    const auto lmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT);
    const auto rmb = engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_RIGHT);
    if (weapon_ && rmb && !check_state_bit(States::ATTACK))
    {
        enable_state_bit(States::TRIGGER_ATTACK);
    }

    if (hit_info.go != ENGINE_INVALID_GAME_OBJECT_ID && lmb)
    {
        if (auto* interactable = my_scene_->get_script<Interactable>(hit_info.go))
        {
            // check distance and interace if close enough
            const auto tc = engineSceneGetTransformComponent(scene, go_);
            const float distance = glm::distance(glm::vec3(hit_info.position.x, hit_info.position.y, hit_info.position.z), glm::vec3(tc.position[0], tc.position[1], tc.position[2]));
            if (distance < 1.0f)
            {
                interactable->interact();
            }
        }
    }

    const auto button_A = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_A);
    const auto button_W = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_W);
    const auto button_D = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_D);
    const auto button_S = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_S);
    if (button_A || button_W || button_D || button_S)
    {
        enable_state_bit(States::MOVE);
    }

    if (weapon_ && engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_F))
    {
        // drop weapnon
        const auto tc = engineSceneGetTransformComponent(scene, go_);
        weapon_->drop_on_ground({ tc.position[0], tc.position[1], tc.position[2] });
        weapon_ = nullptr;
    }

    if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_SPACE) && dodge_data_.can_dodge())
    {
        enable_state_bit(States::DODGE);
        dodge_data_.activate();
    }

    if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_M))
    {
        auto ac = engineSceneGetAnimationControllerComponent(scene, go_);
        engineAnimationControllerPlayback(ac.controller, "1H_Melee_Attack_Slice_Diagonal", dt);
    }


    const auto tc = engineSceneGetTransformComponent(scene, go_);
    const glm::quat rotation = glm::make_quat(tc.rotation);
    const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);

    if (state_ == States::IDLE)
    {
        anim_controller_.set_active_animation("Idle");
    }
    if (check_state_bit(States::DODGE))
    {
        if (!dodge_data_.animation_is_playing())
        {
            clear_state_bit(States::DODGE);
        }
        else
        {
            anim_controller_.set_active_animation("Dodge_Forward");
            const float speed_cooef = 0.015f;
            const float speed = speed_cooef * dt;
            auto tc_dodge = engineSceneGetTransformComponent(scene, go_);
            // move forward // ToDo add doge in other directions
            tc_dodge.position[0] += forward.x * speed;
            //tc.position[1] += forward.y * speed;  // dont go up!
            tc_dodge.position[2] += forward.z * speed;
            engineSceneUpdateTransformComponent(scene, go_, &tc_dodge);
        }
    }
    if (check_state_bit(States::MOVE))
    {
        auto tc_move = engineSceneGetTransformComponent(scene, go_);
        const float speed_cooef = 0.0025f;
        const float speed = speed_cooef * dt; // ToDo: implement diagonal movement speed coef (use pitagoras(?))

        if (button_W)  // up
        {
            tc_move.position[2] -= speed;
        }
        if (button_S) // down
        {
            tc_move.position[2] += speed;
        }
        if (button_A) // left
        {
            tc_move.position[0] -= speed;
        }
        if (button_D) // right
        {
            tc_move.position[0] += speed;
        }

        engineSceneUpdateTransformComponent(scene, go_, &tc_move);

        // compute forard/left/right/backward direction based on mouse position
        const glm::vec3 direction = glm::normalize(glm::vec3(hit_info.position.x, hit_info.position.y, hit_info.position.z) - glm::vec3(tc_move.position[0], tc_move.position[1], tc_move.position[2]));
        // Compute the dot products
        const float forward_dot = glm::dot(direction, forward);
        const float right_dot = glm::dot(direction, right);
        MoveStateData::Direction anim_move_dir = MoveStateData::Direction::eForward;

        // Determine the direction
        if (std::abs(forward_dot) > std::abs(right_dot)) {
            if (forward_dot > 0) {
                anim_move_dir = MoveStateData::Direction::eForward;
            }
            else {
                anim_move_dir = MoveStateData::Direction::eBackward;
            }
        }
        else {
            if (right_dot > 0) {
                anim_move_dir = MoveStateData::Direction::eRight;
            }
            else {
                anim_move_dir = MoveStateData::Direction::eLeft;
            }
        }

        anim_controller_.set_active_animation(move_data_.get_animation_name(anim_move_dir));
        clear_state_bit(States::MOVE);
    }
    if (check_state_bit(States::TRIGGER_ATTACK))
    {
        anim_controller_.set_active_animation(attack_data_.get_animation_name());
        attack_trigger_->activate();
        clear_state_bit(States::TRIGGER_ATTACK);
        enable_state_bit(States::ATTACK);
    }
    if (check_state_bit(States::ATTACK))
    {
        if (!anim_controller_.is_active_animation(attack_data_.get_animation_name()))
        {
            clear_state_bit(States::ATTACK);
            attack_data_ = {};
        }

    }
}

bool project_c::Player::equip_waepon(Weapon* sword)
{
    if (!weapon_ && sword)
    {
        weapon_ = sword;
        const auto right_hand_slot_name = engineSceneGetNameComponent(my_scene_->get_handle(), right_arm_go_);
        weapon_->attach_to_game_object(go_, right_hand_slot_name.name, glm::vec3(0.0f, 0.0f, 0.0f), glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        return true;
    }
    return false;
}

void project_c::Player::add_coin(std::uint64_t amount)
{   
    coins_ += amount;
    // for now only log, but we should update UI
    engineLog(std::format("Player coins: {}\n", coins_).c_str());
}
