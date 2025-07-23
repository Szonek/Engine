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

#include <format>

namespace
{
void attack_event_callback(const engine_animation_event_info_t* info, void* user_data)
{
    assert(info);
    assert(user_data);
    engineLog(std::format("Attack trigger - animation event callback example at time: {}!\n", info->current_time).c_str());
    // i.e. enable trigger:
    //auto* attack_trigger = reinterpret_cast<project_c::AttackTrigger*>(user_data);
    //attack_trigger->activate();
}
} // namespace anonymous

project_c::Weapon::Weapon(engine::IScene* my_scene)
    : BaseNode(my_scene, "weapon-sword")
{
    // transform
    auto tc = engineGameObjectAddTransformComponent(go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    // mesh
    auto mc = engineGameObjectAddMeshComponent(go_);
    mc.geometry = engineGeometryGetByName("Cube.12900");
    assert(mc.geometry != ENGINE_INVALID_OBJECT_HANDLE);
    engineGameObjectUpdateMeshComponent(go_, &mc);

    // material
    auto matc = engineGameObjectAddMaterialComponent(go_);
    set_c_array(matc.data.pong.diffuse_color, std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f});
    matc.data.pong.shininess = 32.0f;
    matc.data.pong.diffuse_texture = engineTextured2DGetByName("barbarian_texture");
    engineGameObjectUpdateMaterialComponent(go_, &matc);

    // physics
    auto cc = engineGameObjectAddColliderComponent(go_);
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
    engineGameObjectUpdateColliderComponent(go_, &cc);

}

project_c::Weapon::~Weapon()
{
}

void project_c::Weapon::attach_to_game_object(engine_game_object_t parent, std::string_view joint_name, std::optional<glm::vec3> position, std::optional<glm::quat> rotation)
{
    // parent to hand
    if (parent != ENGINE_INVALID_GAME_OBJECT_ID)
    {
        auto pc = engineGameObjectAddParentComponent(go_);
        pc.parent = parent;
        engineGameObjectUpdateParentComponent(go_, &pc);
    }

    auto jac = engineGameObjectAddJointAttachmentComponent(go_);
    jac.skin = engineGameObjectGetSkinComponent(parent).skin;
    engineStringSet(jac.joint_name, joint_name.data());
    engineGameObjectUpdateJointAttachmentComponent(go_, &jac);

    if (position.has_value() || rotation.has_value())
    {
        auto tc = engineGameObjectGetTransformComponent(go_);

        if (position.has_value())
        {
            std::memcpy(tc.position, glm::value_ptr(position.value()), sizeof(tc.position));
        }
        if (rotation.has_value())
        {
            std::memcpy(tc.rotation, glm::value_ptr(rotation.value()), sizeof(tc.rotation));
        }     
        // reset scale
        tc.scale[0] = 1.0f;
        tc.scale[1] = 1.0f;
        tc.scale[2] = 1.0f;
        engineGameObjectUpdateTransformComponent(go_, &tc);
    }
}

void project_c::Weapon::drop_on_ground(glm::vec3 position)
{
    // remove joint attachment
    if (engineGameObjectHasJointAttachmentComponent(go_))
    {
        engineGameObjectRemoveJointAttachmentComponent(go_);
    }

    // add rigid body component
    auto rbc = engineGameObjectAddRigidBodyComponent(go_);
    rbc.mass = 1000.0f;
    engineGameObjectUpdateRigidBodyComponent(go_, &rbc);

    // update collider to not be trigger, so it will stop on collision
    auto cc = engineGameObjectGetColliderComponent(go_);
    cc.is_trigger = false;
    engineGameObjectUpdateColliderComponent(go_, &cc);

    // set position
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] = position.x;
    tc.position[1] = position.y + 2.0f;
    tc.position[2] = position.z;
    if (engineGameObjectHasParentComponent(go_))
    {
        const auto parent_tc = engineGameObjectGetTransformComponent(engineGameObjectGetParentComponent(go_).parent);
        tc.scale[0] = parent_tc.scale[0];
        tc.scale[1] = parent_tc.scale[1];
        tc.scale[2] = parent_tc.scale[2];
    }
    engineGameObjectUpdateTransformComponent(go_, &tc);

    // remove parent
    if (engineGameObjectHasParentComponent(go_))
    {
        engineGameObjectRemoveParentComponent(go_);
    }
}

void project_c::Weapon::on_collision(const collision_t& info)
{
    if (auto* floor = my_scene_->get_script<Floor>(info.other))
    {
        // remove rigid body and enable is trigger
        // check if has rigidbody (remove once), because there can be multiple collision calls in single frame
        if (engineGameObjectHasRigidBodyComponent(go_))
        {
            engineGameObjectRemoveRigidBodyComponent(go_);
            auto cc = engineGameObjectGetColliderComponent(go_);
            cc.is_trigger = true;
            engineGameObjectUpdateColliderComponent(go_, &cc);
        }
    }
}

void project_c::Weapon::update(float dt)
{
    auto typed_scene = static_cast<project_c::TestScene*>(my_scene_);

    if (!engineGameObjectHasParentComponent(go_))
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
    // transform
    auto tc = engineGameObjectAddTransformComponent(go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    // physics
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = true;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.transform[1] = 0.21f;
    cc_child.transform[2] = 0.6f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.3f, 0.05f, 0.3f});
    engineGameObjectUpdateColliderComponent(go_, &cc);

    // parent to root
    const auto gos_with_root_name = utils::get_game_objects_with_name("player");
    for (auto& parent : gos_with_root_name)
    {
        if (parent != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            auto pc = engineGameObjectAddParentComponent(go_);
            pc.parent = parent;
            engineGameObjectUpdateParentComponent(go_, &pc);
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
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[1] = -0.25f;
    tc.scale[0] = 0.35f;
    tc.scale[1] = 0.35f;
    tc.scale[2] = 0.35f;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    // physics
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = false;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.transform[0] = 0.0f;
    cc_child.transform[1] = 1.0f;
    cc_child.transform[2] = 0.0f;
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.5f, 1.0f, 0.4f});
    engineGameObjectUpdateColliderComponent(go_, &cc);

    //rb
    auto rbc = engineGameObjectAddRigidBodyComponent(go_);
    rbc.mass = 100000.0f;
    engineGameObjectUpdateRigidBodyComponent(go_, &rbc);

    // delete objects not needed at creation time
    engineGameObjectDestroy(utils::get_game_objects_with_name("1H_Axe")[0]);
    engineGameObjectDestroy(utils::get_game_objects_with_name("2H_Axe")[0]);
    engineGameObjectDestroy(utils::get_game_objects_with_name("Mug")[0]);
    engineGameObjectDestroy(utils::get_game_objects_with_name("Barbarian_Round_Shield")[0]);
    engineGameObjectDestroy(utils::get_game_objects_with_name("1H_Axe_Offhand")[0]);

    // add attack trigger
    attack_trigger_ = my_scene_->register_script<AttackTrigger>(engineGameObjectCreate());

    // set animation layers
    auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;
    engineAnimationControllerLayerSetWeight(animation_controller, LOCOMOTION_LAYER_ID, 0.1f); // default layer (lower body layer)
    engineAnimationControllerAddLayer(animation_controller, COMBAT_LAYER_ID, 1.0f);     // upper body layer
    engineAnimationControllerSetMode(animation_controller, COMBAT_LAYER_ID, ENGINE_ANIMATION_LAYER_MODE_ADDITIVE);

    // set event for attack trigger - just an example to test feature (2 animation events triggering at the same time)
    float attack_duration = 0.0f;
    engineAnimationControllerAnimationGetDuration(animation_controller, attack_data_.get_animation_name(), &attack_duration);
    engine_animation_event_t ev{};
    ev.trigger_time = 0.5f * attack_duration;  // activate in middle of animation
    ev.user_data = &attack_trigger_;
    ev.fn_ptr = attack_event_callback;
    engine_animation_event_id_t ev_id = ENGINE_INVALID_OBJECT_HANDLE;
    engineAnimationControllerAnimationAddEvent(animation_controller, attack_data_.get_animation_name(), ev, &ev_id);
    assert(ev_id != ENGINE_INVALID_OBJECT_HANDLE);
    engine_animation_event_id_t ev2_id = ENGINE_INVALID_OBJECT_HANDLE;
    engineAnimationControllerAnimationAddEvent(animation_controller, attack_data_.get_animation_name(), ev, &ev2_id);
    assert(ev2_id != ENGINE_INVALID_OBJECT_HANDLE);
    assert(ev_id != ev2_id);
}

void project_c::Player::update(float dt)
{
    engine::ScopedProfiler prof("project_c::Player::update");
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

    auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;

    // [DEBUG] reset position  ToDo: remove it later
    if (engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_0))
    {
        set_world_position( 0.0f, 1.0f, 0.0f );
    }

    const std::array<engine_game_object_t, 1> raycast_ignore_list = { attack_trigger_->get_game_object() };
    const auto active_camera_go = utils::get_active_camera_game_objects()[0];
    const auto ray = utils::get_ray_from_mouse_position(active_camera_go);
    const auto hit_info = enginePhysicsRayCast(raycast_ignore_list.data(), raycast_ignore_list.size(), &ray, 1000000.0f);

    auto rotate_towards_global_target = [&]()
        {
            auto tc = engineGameObjectGetTransformComponent(go_);
            auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(hit_info.position.x, hit_info.position.y, hit_info.position.z));
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            engineGameObjectUpdateTransformComponent(go_, &tc);
        };
    rotate_towards_global_target();// rotate towards target

    const auto lmb = engineMouseIsButtonDown(ENGINE_MOUSE_BUTTON_LEFT);
    const auto rmb = engineMouseIsButtonDown(ENGINE_MOUSE_BUTTON_RIGHT);
    if (weapon_ && rmb && !check_state_bit(States::ATTACK))
    {
        enable_state_bit(States::TRIGGER_ATTACK);
    }

    if (hit_info.go != ENGINE_INVALID_GAME_OBJECT_ID && lmb)
    {
        if (auto* interactable = my_scene_->get_script<Interactable>(hit_info.go))
        {
            // check distance and interface if close enough
            const auto tc = engineGameObjectGetTransformComponent(go_);
            const float distance = glm::distance(glm::vec3(hit_info.position.x, hit_info.position.y, hit_info.position.z), glm::vec3(tc.position[0], tc.position[1], tc.position[2]));
            if (distance < 1.0f)
            {
                interactable->interact();
            }
        }
    }

    const auto button_A = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_A);
    const auto button_W = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_W);
    const auto button_D = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_D);
    const auto button_S = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_S);
    if (button_A || button_W || button_D || button_S)
    {
        enable_state_bit(States::MOVE);
    }

    if (weapon_ && engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_F))
    {
        // drop weapon
        const auto tc = engineGameObjectGetTransformComponent(go_);
        weapon_->drop_on_ground({ tc.position[0], tc.position[1], tc.position[2] });
        weapon_ = nullptr;
    }

    const auto tc = engineGameObjectGetTransformComponent(go_);
    const glm::quat rotation = glm::make_quat(tc.rotation);
    const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);

    if (state_ == States::IDLE)
    {
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, "Unarmed_Idle"))
        {
            engineAnimationControllerAnimationCrossFade(animation_controller, "Unarmed_Idle", LOCOMOTION_LAYER_ID, 0.15f);
        }
    }
    if (check_state_bit(States::MOVE))
    {
        auto tc_move = engineGameObjectGetTransformComponent(go_);
        const float speed_cooef = 0.0025f;
        const float speed = speed_cooef * dt; // ToDo: implement diagonal movement speed coef (use pitagoras(?))
       
        // Compute intended movement direction based on input
        glm::vec3 move_dir(0.0f);
        if (button_W) move_dir.z -= 1.0f; // up
        if (button_S) move_dir.z += 1.0f; // down
        if (button_D) move_dir.x += 1.0f; // right
        if (button_A) move_dir.x -= 1.0f; // left

        MoveStateData::Direction anim_move_dir = MoveStateData::Direction::eForward;
        if (glm::length(move_dir) > 0.0f)
        {
            move_dir = glm::normalize(move_dir);

            // 2. Move in world space
            tc_move.position[0] += move_dir.x * speed;
            tc_move.position[2] += move_dir.z * speed;
            engineGameObjectUpdateTransformComponent(go_, &tc_move);

            // 3. Compute facing (character's forward in world space)
            const auto tc = engineGameObjectGetTransformComponent(go_);
            glm::quat facing = glm::make_quat(tc.rotation);
            glm::vec3 char_forward = facing * glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec3 char_right = facing * glm::vec3(1.0f, 0.0f, 0.0f);

            // 4. Project move_dir onto character's local axes to determine anim direction
            float forward_dot = glm::dot(move_dir, glm::normalize(char_forward));
            float right_dot = glm::dot(move_dir, glm::normalize(char_right));

            if (std::abs(forward_dot) > std::abs(right_dot))
            {
                anim_move_dir = (forward_dot > 0) ? MoveStateData::Direction::eForward : MoveStateData::Direction::eBackward;
            }
            else 
            {
                anim_move_dir = (right_dot > 0) ? MoveStateData::Direction::eRight : MoveStateData::Direction::eLeft;
            }
        }

        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, move_data_.get_animation_name(anim_move_dir)))
        {
            engineAnimationControllerAnimationCrossFade(animation_controller, move_data_.get_animation_name(anim_move_dir), LOCOMOTION_LAYER_ID, 0.2f);
        }

        clear_state_bit(States::MOVE);
    }
    if (check_state_bit(States::TRIGGER_ATTACK))
    {
        engineAnimationControllerAnimationPlay(animation_controller, attack_data_.get_animation_name(), COMBAT_LAYER_ID);
        attack_trigger_->activate();
        clear_state_bit(States::TRIGGER_ATTACK);
        enable_state_bit(States::ATTACK);
    }
    if (check_state_bit(States::ATTACK))
    {
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, attack_data_.get_animation_name()))
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
        weapon_->attach_to_game_object(go_, "handslot.r", glm::vec3(0.0f, 0.0f, 0.0f), glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
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

const char* project_c::Player::MoveStateData::direction_to_string(Direction dir) const
{
    switch (dir) 
    {
    case MoveStateData::Direction::eForward:  return "Forward";
    case MoveStateData::Direction::eBackward: return "Backward";
    case MoveStateData::Direction::eLeft:     return "Left";
    case MoveStateData::Direction::eRight:    return "Right";
    default:                                  return "Unknown";
    }
}

const char* project_c::Player::MoveStateData::get_animation_name(Direction dir) const
{
    //engineLog(std::format("anim_move_dir: {}\n", direction_to_string(dir)).c_str());
    switch (dir)
    {
    case Direction::eForward:
        return "Running_A";
    case Direction::eBackward:
        return "Running_A";
    case Direction::eLeft:
        return "Running_Strafe_Left";
    case Direction::eRight:
        return "Running_Strafe_Right";
    default:
        assert(!"Unknown move direction for player!");
    }
    return "";
}

const char* project_c::Player::AttackStateData::get_animation_name() const
{
    //return "1H_Melee_Attack_Chop";
    //return "1H_Melee_Attack_Slice_Horizontal";
    //return "1H_Melee_Attack_Stab";
    return "1H_Melee_Attack_Slice_Diagonal";
}