#include "player_script.h"
#include "scripts_utils.h"
#include "enemy_script.h"
#include "enviorment_script.h"
#include "interactable_script.h"
#include "camera_script.h"
#include "building_script.h"
#include "../scenes/scene_test.h"

#include "../app.h"
#include "iscene.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <format>

namespace
{
constexpr float ATTACK_PLAYBACK_RATE = 1.25f;
constexpr float ATTACK_INPUT_BUFFER_SECONDS = 0.25f;
constexpr float ATTACK_COMBO_CROSS_FADE_SECONDS = 0.08f;
constexpr float ATTACK_EXIT_CROSS_FADE_SECONDS = 0.15f;
constexpr float PLAYER_TURN_RATE_DEGREES_PER_SECOND = 540.0f;
constexpr float PLAYER_WALK_SPEED = 1.875f;
constexpr float PLAYER_RUN_SPEED = 2.5f;
constexpr float PLAYER_MOVEMENT_ACCELERATION = 24.0f;
constexpr float PLAYER_MOVEMENT_BRAKING = 30.0f;
constexpr float PLAYER_IDLE_SPEED_THRESHOLD = 0.05f;
constexpr float HIT_STOP_DURATION_SECONDS = 0.06f;
constexpr float HIT_CAMERA_SHAKE_INTENSITY = 0.035f;
constexpr float HIT_CAMERA_SHAKE_DURATION_SECONDS = 0.08f;
constexpr float DODGE_INVULNERABILITY_SECONDS = 0.20f;
constexpr float DODGE_COOLDOWN_SECONDS = 0.65f;
constexpr float DODGE_SPEED = 4.0f;
constexpr float PLAYER_HIT_STUN_SECONDS = 0.0f;
constexpr float PLAYER_KNOCKBACK_SPEED = 2.5f;
constexpr float PLAYER_MASS = 100000.0f;

// Traversal. Heights are measured from the player's feet (transform origin), player is ~0.85 tall.
constexpr float PLAYER_HEIGHT = 0.85f;
constexpr float PLAYER_HALF_WIDTH = 0.18f;
constexpr float PROBE_KNEE_HEIGHT = 0.3f;
constexpr float PROBE_CHEST_HEIGHT = 0.7f;
constexpr float PROBE_HEAD_HEIGHT = 0.95f;
constexpr float TRAVERSAL_DETECT_RANGE = 1.3f;          // obstacles this close in the movement direction show the marker
constexpr float TRAVERSAL_TRIGGER_DISTANCE = 0.35f;     // traversal starts automatically this close to the obstacle
constexpr float TRAVERSAL_MIN_APPROACH_DOT = 0.6f;      // movement has to point into the wall (not just walk along it)
constexpr float TRAVERSAL_REGRAB_COOLDOWN = 0.3f;
constexpr float TRAVERSAL_DROP_REGRAB_COOLDOWN = 0.8f;  // after letting go with SPACE
constexpr float CLIMB_INPUT_DEADZONE = 0.3f;
constexpr float CLIMB_WALL_OFFSET = 0.2f;               // distance between wall surface and the player while climbing
constexpr float CLIMB_PROBE_DEPTH = 0.25f;              // how far behind the expected wall surface probes still count as a hit
constexpr float CLIMB_SPEED_VERTICAL = 2.0f;
constexpr float CLIMB_SPEED_LATERAL = 1.6f;
constexpr float CLIMB_DROP_PUSH = 0.15f;
constexpr float MANTLE_LEDGE_DEPTH = 0.25f;             // how far behind the wall surface ledge is looked for
constexpr float MANTLE_MAX_LEDGE_HEIGHT = 1.4f;
constexpr float MANTLE_DURATION = 0.4f;
constexpr float VAULT_MAX_HEIGHT = 0.7f;
constexpr float VAULT_MAX_DEPTH = 1.0f;
constexpr float VAULT_CLEARANCE = 0.12f;
constexpr float VAULT_LANDING_DISTANCE = 0.35f;
constexpr float VAULT_MAX_ALIGNMENT = 0.6f;
constexpr float VAULT_DURATION = 0.45f;
constexpr float CLIMB_DOWN_EDGE_DISTANCE = 0.45f;
constexpr float CLIMB_DOWN_HANG_DEPTH = 0.95f;
constexpr float CLIMB_DOWN_DURATION = 0.35f;
// No dedicated climbing clips in the KayKit pack, closest matches are used.
constexpr const char* CLIMB_ANIMATION = "Jump_Idle";
constexpr const char* VAULT_ANIMATION = "Jump_Full_Short";
constexpr const char* MANTLE_ANIMATION = "Jump_Full_Short";
constexpr float CLIMB_ANIMATION_RATE_MOVING = 1.6f;
constexpr float CLIMB_ANIMATION_RATE_IDLE = 0.2f;
constexpr float TRAVERSAL_MARKER_SIZE = 0.05f;           // Primitive_Cube_Small is 2 units big, pivot at the bottom

const glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);

glm::vec3 flatten(const glm::vec3& v)
{
    const glm::vec3 flat(v.x, 0.0f, v.z);
    const float length = glm::length(flat);
    return length > 0.0001f ? flat / length : glm::vec3(0.0f);
}

float yaw_from_direction(const glm::vec3& direction)
{
    return std::atan2(direction.x, direction.z);
}

void attack_window_open_callback(const engine_animation_event_info_t* info, void* user_data)
{
    assert(info);
    assert(user_data);
    static_cast<project_c::AttackTrigger*>(user_data)->activate();
}

void attack_window_close_callback(const engine_animation_event_info_t* info, void* user_data)
{
    assert(info);
    assert(user_data);
    static_cast<project_c::AttackTrigger*>(user_data)->deactivate();
}
} // namespace anonymous

project_c::Weapon::Weapon(engine::IScene* my_scene)
    : BaseNode(my_scene, "weapon-daggers")
{
    const auto dagger_texture = engineTextured2DGetByName("rogue_texture");

    // transform
    auto tc = engineGameObjectAddTransformComponent(go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    // mesh
    auto mc = engineGameObjectAddMeshComponent(go_);
    mc.geometry = engineGeometryGetByName("Rogue.glb#Cylinder.008");
    assert(mc.geometry != ENGINE_INVALID_OBJECT_HANDLE);
    engineGameObjectUpdateMeshComponent(go_, &mc);

    // material
    auto matc = engineGameObjectAddMaterialComponent(go_);
    set_c_array(matc.data.pong.diffuse_color, std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f});
    matc.data.pong.shininess = 32.0f;
    matc.data.pong.diffuse_texture = dagger_texture;
    engineGameObjectUpdateMaterialComponent(go_, &matc);

    // offhand dagger
    offhand_go_ = engineGameObjectCreate();
    auto offhand_nc = engineGameObjectAddNameComponent(offhand_go_);
    std::strncpy(offhand_nc.name, "weapon-daggers-offhand", std::size(offhand_nc.name));
    engineGameObjectUpdateNameComponent(offhand_go_, &offhand_nc);

    auto offhand_tc = engineGameObjectAddTransformComponent(offhand_go_);
    engineGameObjectUpdateTransformComponent(offhand_go_, &offhand_tc);

    auto offhand_mc = engineGameObjectAddMeshComponent(offhand_go_);
    offhand_mc.geometry = engineGeometryGetByName("Rogue.glb#Cylinder.002");
    assert(offhand_mc.geometry != ENGINE_INVALID_OBJECT_HANDLE);
    engineGameObjectUpdateMeshComponent(offhand_go_, &offhand_mc);

    auto offhand_matc = engineGameObjectAddMaterialComponent(offhand_go_);
    offhand_matc = matc;
    engineGameObjectUpdateMaterialComponent(offhand_go_, &offhand_matc);

    attach_offhand_to_main();

    // physics
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.bounciness = 0.35f;
    cc.friction_static = 0.9f;
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = true;
    auto& cc_child = cc.collider.compound.children[0];
    cc_child.rotation_quaternion[3] = 1.0f;
    cc_child.transform[0] = 0.0f;
    cc_child.transform[1] = 0.35f;
    cc_child.transform[2] = 0.0f;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    // ToDo: box sie could be smaller and only increase box size when item dropped on the ground, so it do not fly trhoguh ground
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.1f, 0.10f, 0.04f});
    engineGameObjectUpdateColliderComponent(go_, &cc);

}

project_c::Weapon::~Weapon()
{
    // the offhand dagger is not a script and, while equipped, not a child of the weapon either, so nothing else destroys it
    if (engineGameObjectIsValid(offhand_go_))
    {
        engineGameObjectDestroy(offhand_go_);
    }
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

void project_c::Weapon::attach_offhand_to_game_object(engine_game_object_t parent, std::string_view joint_name)
{
    if (engineGameObjectHasJointAttachmentComponent(offhand_go_))
    {
        engineGameObjectRemoveJointAttachmentComponent(offhand_go_);
    }
    if (engineGameObjectHasParentComponent(offhand_go_))
    {
        engineGameObjectRemoveParentComponent(offhand_go_);
    }
    auto pc = engineGameObjectAddParentComponent(offhand_go_);
    pc.parent = parent;
    engineGameObjectUpdateParentComponent(offhand_go_, &pc);

    auto jac = engineGameObjectAddJointAttachmentComponent(offhand_go_);
    jac.skin = engineGameObjectGetSkinComponent(parent).skin;
    engineStringSet(jac.joint_name, joint_name.data());
    engineGameObjectUpdateJointAttachmentComponent(offhand_go_, &jac);

    auto tc = engineGameObjectGetTransformComponent(offhand_go_);
    set_c_array(tc.position, std::array<float, 3>{ 0.0f, 0.0f, 0.0f });
    set_c_array(tc.rotation, std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });
    engineGameObjectUpdateTransformComponent(offhand_go_, &tc);
}

void project_c::Weapon::attach_offhand_to_main()
{
    if (engineGameObjectHasJointAttachmentComponent(offhand_go_))
    {
        engineGameObjectRemoveJointAttachmentComponent(offhand_go_);
    }
    if (engineGameObjectHasParentComponent(offhand_go_))
    {
        engineGameObjectRemoveParentComponent(offhand_go_);
    }
    auto pc = engineGameObjectAddParentComponent(offhand_go_);
    pc.parent = go_;
    engineGameObjectUpdateParentComponent(offhand_go_, &pc);

    // lay it next to the main dagger
    auto tc = engineGameObjectGetTransformComponent(offhand_go_);
    set_c_array(tc.position, std::array<float, 3>{ 0.3f, 0.0f, 0.0f });
    set_c_array(tc.rotation, std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });
    engineGameObjectUpdateTransformComponent(offhand_go_, &tc);
}

void project_c::Weapon::drop_on_ground(glm::vec3 position)
{
    attach_offhand_to_main();

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
        // remove rigid body and enable the trigger
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

project_c::AttackTrigger::AttackTrigger(engine::IScene* my_scene, engine_game_object_t go, Player* player)
    : BaseNode(my_scene, go, "attack-trigger")
    , player_(player)
{
    assert(player_);
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
    if (is_active_ && !hit_enemies_.contains(info.other))
    {
        if (auto* enemy = my_scene_->get_script<Enemy>(info.other))
        {
            hit_enemies_.insert(info.other);
            enemy->receive_attack_hit(10, HIT_STOP_DURATION_SECONDS);
            player_->on_confirmed_attack_hit(HIT_STOP_DURATION_SECONDS);
        }
    }
}

void project_c::AttackTrigger::begin_swing()
{
    is_active_ = false;
    hit_enemies_.clear();
}

void project_c::AttackTrigger::activate()
{
    // every hit window of the swing can hit the same enemy again (i.e. one hit per dagger)
    hit_enemies_.clear();
    is_active_ = true;
}

void project_c::AttackTrigger::deactivate()
{
    is_active_ = false;
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
    set_kinematic(false);

    create_traversal_marker();

    // delete objects not needed at creation time
    engineGameObjectDestroy(utils::get_game_objects_with_name("Knife")[0]);
    engineGameObjectDestroy(utils::get_game_objects_with_name("Knife_Offhand")[0]);
    engineGameObjectDestroy(utils::get_game_objects_with_name("1H_Crossbow")[0]);
    engineGameObjectDestroy(utils::get_game_objects_with_name("2H_Crossbow")[0]);
    engineGameObjectDestroy(utils::get_game_objects_with_name("Throwable")[0]);

    // add attack trigger
    attack_trigger_ = my_scene_->register_script<AttackTrigger>(engineGameObjectCreate(), this);

    // set animation layers
    auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;
    engineAnimationControllerLayerSetWeight(animation_controller, LOCOMOTION_LAYER_ID, 1.0f);
    const bool locomotion_loops = engineAnimationControllerLayerSetLooping(animation_controller, LOCOMOTION_LAYER_ID, true);
    assert(locomotion_loops);
    engineAnimationControllerAddLayer(animation_controller, COMBAT_LAYER_ID, 9.0f);
    engineAnimationControllerSetMode(animation_controller, COMBAT_LAYER_ID, ENGINE_ANIMATION_LAYER_MODE_OVERRIDE);
    const bool pelvis_masked = engineAnimationControllerLayerSetJointWeight(animation_controller, COMBAT_LAYER_ID, "hips", 0.15f);
    const bool left_leg_masked = engineAnimationControllerLayerSetJointWeight(animation_controller, COMBAT_LAYER_ID, "upperleg.l", 0.0f);
    const bool right_leg_masked = engineAnimationControllerLayerSetJointWeight(animation_controller, COMBAT_LAYER_ID, "upperleg.r", 0.0f);
    const bool upper_body_masked = engineAnimationControllerLayerSetJointWeight(animation_controller, COMBAT_LAYER_ID, "spine", 1.0f);
    assert(pelvis_masked && left_leg_masked && right_leg_masked && upper_body_masked);

    for (std::size_t combo_index = 0; combo_index < AttackStateData::combo_length; ++combo_index)
    {
        const auto& attack = attack_data_.get_attack(combo_index);
        float duration = 0.0f;
        engineAnimationControllerAnimationGetDuration(animation_controller, attack.animation_name, &duration);
        assert(duration > 0.0f);

        for (std::size_t window_index = 0; window_index < attack.hit_windows_count; ++window_index)
        {
            const auto& window = attack.hit_windows[window_index];
            assert(window.start < window.end && window.end < duration);

            engine_animation_event_t open_event{};
            open_event.trigger_time = window.start;
            open_event.user_data = attack_trigger_;
            open_event.fn_ptr = attack_window_open_callback;
            engine_animation_event_id_t open_event_id = ENGINE_INVALID_OBJECT_HANDLE;
            const auto open_result = engineAnimationControllerAnimationAddEvent(animation_controller, attack.animation_name, open_event, &open_event_id);
            assert(open_result == ENGINE_RESULT_CODE_OK && open_event_id != ENGINE_INVALID_OBJECT_HANDLE);

            engine_animation_event_t close_event{};
            close_event.trigger_time = window.end;
            close_event.user_data = attack_trigger_;
            close_event.fn_ptr = attack_window_close_callback;
            engine_animation_event_id_t close_event_id = ENGINE_INVALID_OBJECT_HANDLE;
            const auto close_result = engineAnimationControllerAnimationAddEvent(animation_controller, attack.animation_name, close_event, &close_event_id);
            assert(close_result == ENGINE_RESULT_CODE_OK && close_event_id != ENGINE_INVALID_OBJECT_HANDLE);
        }
    }
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
    if (check_state_bit(States::DEAD))
    {
        return;
    }
    dodge_cooldown_remaining_seconds_ = std::max(0.0f, dodge_cooldown_remaining_seconds_ - dt / 1000.0f);

    auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;
    if (hit_stop_remaining_seconds_ > 0.0f)
    {
        if (hit_stop_just_started_)
        {
            hit_stop_just_started_ = false;
        }
        else
        {
            hit_stop_remaining_seconds_ = std::max(0.0f, hit_stop_remaining_seconds_ - dt / 1000.0f);
        }

        if (hit_stop_remaining_seconds_ > 0.0f)
        {
            return;
        }
        if (!engineAnimationControllerSetPaused(animation_controller, false))
        {
            engineLog("Failed to resume player animation after hit-stop.\n");
        }
    }

    // [DEBUG] reset position  ToDo: remove it later
    if (engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_0))
    {
        set_world_position( 0.0f, 1.0f, 0.0f );
    }

    const std::array<engine_game_object_t, 1> raycast_ignore_list = { attack_trigger_->get_game_object() };
    const auto active_camera_go = utils::get_active_camera_game_objects()[0];
    const auto ray = utils::get_ray_from_mouse_position(active_camera_go);
    const auto hit_info = enginePhysicsRayCast(raycast_ignore_list.data(), raycast_ignore_list.size(), &ray, 1000000.0f);

    const auto lmb = engineMouseIsButtonDown(ENGINE_MOUSE_BUTTON_LEFT);
    const auto rmb = engineMouseIsButtonDown(ENGINE_MOUSE_BUTTON_RIGHT);
    const bool attack_pressed = rmb && !attack_input_was_down_;
    attack_input_was_down_ = rmb;
    const auto space_down = engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_SPACE);
    const bool dodge_pressed = space_down && !dodge_input_was_down_;
    dodge_input_was_down_ = space_down;
    traversal_regrab_cooldown_ = std::max(0.0f, traversal_regrab_cooldown_ - dt / 1000.0f);

    if (traversal_state_ != TraversalState::eNone)
    {
        update_traversal_marker({}, dt / 1000.0f);
        // space lets go of the wall
        update_traversal(animation_controller, dt / 1000.0f, dodge_pressed);
        return;
    }

    if (weapon_ && attack_pressed && !dodge_pressed && dodge_remaining_seconds_ <= 0.0f && hit_stun_remaining_seconds_ <= 0.0f)
    {
        if (!check_state_bit(States::ATTACK))
        {
            start_attack(animation_controller, 0);
        }
        else if (attack_combo_index_ + 1 < AttackStateData::combo_length)
        {
            attack_buffered_ = true;
            attack_buffer_remaining_ = ATTACK_INPUT_BUFFER_SECONDS;
        }
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

    glm::vec2 move_input(0.0f);
    if (button_A) move_input.x -= 1.0f;
    if (button_D) move_input.x += 1.0f;
    if (button_W) move_input.y -= 1.0f;
    if (button_S) move_input.y += 1.0f;
    if (glm::length(move_input) > 1.0f)
    {
        move_input = glm::normalize(move_input);
    }

    // walking into a wall / window / roof edge: show marker while approaching, start traversal on contact
    const glm::vec3 player_position(tc.position[0], tc.position[1], tc.position[2]);
    TraversalPlan traversal_plan{};
    if (glm::length(move_input) > 0.0f
        && !check_state_bit(States::ATTACK)
        && dodge_remaining_seconds_ <= 0.0f
        && hit_stun_remaining_seconds_ <= 0.0f)
    {
        traversal_plan = find_traversal(player_position, flatten(glm::vec3(move_input.x, 0.0f, move_input.y)));
    }
    // marker stays visible whenever player is close to something traversable, even when standing still or walking along it
    update_traversal_marker(traversal_plan.type != TraversalPlan::Type::eNone ? traversal_plan : find_nearest_traversal(player_position), dt / 1000.0f);
    if (traversal_plan.type != TraversalPlan::Type::eNone
        && traversal_plan.distance <= TRAVERSAL_TRIGGER_DISTANCE
        && traversal_regrab_cooldown_ <= 0.0f)
    {
        execute_traversal(animation_controller, traversal_plan);
        return;
    }

    if (dodge_pressed
        && dodge_cooldown_remaining_seconds_ <= 0.0f
        && hit_stun_remaining_seconds_ <= 0.0f
        && !check_state_bit(States::ATTACK))
    {
        start_dodge(animation_controller, move_input, rotation);
    }

    auto movement_transform = engineGameObjectGetTransformComponent(go_);
    const float delta_seconds = dt / 1000.0f;
    bool dodging_this_frame = dodge_remaining_seconds_ > 0.0f;
    float movement_speed = 0.0f;
    if (dodging_this_frame)
    {
        movement_velocity_ = glm::vec2(0.0f);
        movement_transform.position[0] += dodge_direction_.x * DODGE_SPEED * delta_seconds;
        movement_transform.position[2] += dodge_direction_.z * DODGE_SPEED * delta_seconds;
        dodge_remaining_seconds_ = std::max(0.0f, dodge_remaining_seconds_ - delta_seconds);
        dodge_invulnerability_remaining_seconds_ = std::max(0.0f, dodge_invulnerability_remaining_seconds_ - delta_seconds);
        if (dodge_remaining_seconds_ <= 0.0f)
        {
            const bool locomotion_loops = engineAnimationControllerLayerSetLooping(animation_controller, LOCOMOTION_LAYER_ID, true);
            const bool combat_layer_restored = engineAnimationControllerLayerSetWeight(animation_controller, COMBAT_LAYER_ID, 1.0f);
            assert(locomotion_loops && combat_layer_restored);
        }
    }
    else if (hit_stun_remaining_seconds_ > 0.0f)
    {
        movement_velocity_ = glm::vec2(0.0f);
        movement_transform.position[0] += knockback_velocity_.x * delta_seconds;
        movement_transform.position[2] += knockback_velocity_.z * delta_seconds;
        knockback_velocity_ *= std::exp(-10.0f * delta_seconds);
        hit_stun_remaining_seconds_ = std::max(0.0f, hit_stun_remaining_seconds_ - delta_seconds);
    }
    else
    {
        const float target_speed = PLAYER_RUN_SPEED;
        const glm::vec2 target_velocity = move_input * target_speed;
        const glm::vec2 velocity_delta = target_velocity - movement_velocity_;
        const float max_velocity_change = (glm::length(move_input) > 0.0f ? PLAYER_MOVEMENT_ACCELERATION : PLAYER_MOVEMENT_BRAKING) * delta_seconds;
        const float velocity_delta_length = glm::length(velocity_delta);
        if (velocity_delta_length > max_velocity_change && velocity_delta_length > 0.0f)
        {
            movement_velocity_ += velocity_delta * (max_velocity_change / velocity_delta_length);
        }
        else
        {
            movement_velocity_ = target_velocity;
        }

        movement_speed = glm::length(movement_velocity_);
        if (movement_speed < PLAYER_IDLE_SPEED_THRESHOLD && glm::length(move_input) == 0.0f)
        {
            movement_velocity_ = glm::vec2(0.0f);
            movement_speed = 0.0f;
        }
        movement_transform.position[0] += movement_velocity_.x * delta_seconds;
        movement_transform.position[2] += movement_velocity_.y * delta_seconds;
    }
    engineGameObjectUpdateTransformComponent(go_, &movement_transform);

    if (!dodging_this_frame && movement_speed > PLAYER_IDLE_SPEED_THRESHOLD)
    {
        const glm::vec3 move_direction(movement_velocity_.x / movement_speed, 0.0f, movement_velocity_.y / movement_speed);
        const glm::vec3 character_forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
        const glm::vec3 character_right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        const float forward_dot = glm::dot(move_direction, glm::normalize(character_forward));
        const float right_dot = glm::dot(move_direction, glm::normalize(character_right));

        MoveStateData::Direction animation_direction = MoveStateData::Direction::eForward;
        if (std::abs(forward_dot) > std::abs(right_dot))
        {
            animation_direction = forward_dot > 0.0f ? MoveStateData::Direction::eForward : MoveStateData::Direction::eBackward;
        }
        else if (!dodging_this_frame)
        {
            animation_direction = right_dot > 0.0f ? MoveStateData::Direction::eRight : MoveStateData::Direction::eLeft;
        }

        const auto animation_name = move_data_.get_animation_name(animation_direction);
        const float reference_speed = move_data_.get_animation_reference_speed(animation_direction);
        const float playback_rate = std::clamp(movement_speed / reference_speed, 0.05f, 1.35f);
        const bool playback_rate_set = engineAnimationControllerLayerSetPlaybackRate(animation_controller, LOCOMOTION_LAYER_ID, playback_rate);
        assert(playback_rate_set);

        if (!engineAnimationControllerIsAnimationPlayingOnLayer(animation_controller, animation_name, LOCOMOTION_LAYER_ID))
        {
            engineAnimationControllerAnimationCrossFade(animation_controller, animation_name, LOCOMOTION_LAYER_ID, 0.15f);
        }
    }
    else
    {
        const bool playback_rate_set = engineAnimationControllerLayerSetPlaybackRate(animation_controller, LOCOMOTION_LAYER_ID, 1.0f);
        assert(playback_rate_set);
        if (!engineAnimationControllerIsAnimationPlayingOnLayer(animation_controller, "Unarmed_Idle", LOCOMOTION_LAYER_ID))
        {
            engineAnimationControllerAnimationCrossFade(animation_controller, "Unarmed_Idle", LOCOMOTION_LAYER_ID, 0.15f);
        }
    }
    if (check_state_bit(States::ATTACK))
    {
        attack_elapsed_ += ATTACK_PLAYBACK_RATE * dt / 1000.0f;
        if (attack_buffered_)
        {
            attack_buffer_remaining_ -= dt / 1000.0f;
            if (attack_buffer_remaining_ <= 0.0f)
            {
                attack_buffered_ = false;
            }
        }

        const auto& attack = attack_data_.get_attack(attack_combo_index_);
        const bool in_hit_window = std::any_of(attack.hit_windows.begin(), attack.hit_windows.begin() + attack.hit_windows_count,
            [this](const AttackStateData::HitWindow& window) { return attack_elapsed_ >= window.start && attack_elapsed_ < window.end; });
        if (in_hit_window)
        {
            attack_phase_ = AttackPhase::eActive;
        }
        else if (attack_elapsed_ >= attack.hit_windows[attack.hit_windows_count - 1].end)
        {
            attack_phase_ = AttackPhase::eRecovery;
        }
        else
        {
            // also between the hits, so player can adjust facing before second dagger lands
            attack_phase_ = AttackPhase::eWindup;
        }

        const bool attack_is_playing = engineAnimationControllerIsAnimationPlayingOnLayer(animation_controller, attack.animation_name, COMBAT_LAYER_ID);
        const bool combo_window_open = attack_elapsed_ >= attack.combo_window_start && attack_elapsed_ < attack.combo_window_end;
        if (attack_buffered_ && combo_window_open)
        {
            attack_buffered_ = false;
            start_attack(animation_controller, attack_combo_index_ + 1);
        }
        else if (!attack_is_playing || attack_elapsed_ >= attack.exit_time)
        {
            if (attack_buffered_ && attack_combo_index_ + 1 < AttackStateData::combo_length)
            {
                attack_buffered_ = false;
                start_attack(animation_controller, attack_combo_index_ + 1);
            }
            else
            {
                clear_state_bit(States::ATTACK);
                attack_phase_ = AttackPhase::eIdle;
                attack_trigger_->deactivate();
                attack_buffered_ = false;
                const bool combat_rate_reset = engineAnimationControllerLayerSetPlaybackRate(animation_controller, COMBAT_LAYER_ID, 1.0f);
                assert(combat_rate_reset);
                if (engineAnimationControllerAnimationCrossFade(animation_controller, "Unarmed_Idle", COMBAT_LAYER_ID, ATTACK_EXIT_CROSS_FADE_SECONDS))
                {
                    const bool combat_idle_loops = engineAnimationControllerLayerSetLooping(animation_controller, COMBAT_LAYER_ID, true);
                    assert(combat_idle_loops);
                }
                else
                {
                    engineLog("Failed to cross-fade combat layer back to idle.\n");
                }
            }
        }
    }

    if (attack_phase_ != AttackPhase::eActive && !dodging_this_frame && hit_stun_remaining_seconds_ <= 0.0f)
    {
        auto facing_transform = engineGameObjectGetTransformComponent(go_);
        const glm::vec3 origin(facing_transform.position[0], facing_transform.position[1], facing_transform.position[2]);
        const glm::vec3 target(hit_info.position.x, hit_info.position.y, hit_info.position.z);
        const glm::vec3 target_direction(target.x - origin.x, 0.0f, target.z - origin.z);
        if (glm::dot(target_direction, target_direction) > 0.000001f)
        {
            const glm::quat current_rotation = glm::normalize(glm::make_quat(facing_transform.rotation));
            const glm::vec3 current_forward = current_rotation * glm::vec3(0.0f, 0.0f, 1.0f);
            const float current_yaw = std::atan2(current_forward.x, current_forward.z);
            const float target_yaw = std::atan2(target_direction.x, target_direction.z);
            const float yaw_delta = std::atan2(std::sin(target_yaw - current_yaw), std::cos(target_yaw - current_yaw));
            const float max_turn = glm::radians(PLAYER_TURN_RATE_DEGREES_PER_SECOND) * (dt / 1000.0f);
            const float turn_step = std::clamp(yaw_delta, -max_turn, max_turn);
            const glm::quat rotation = glm::angleAxis(current_yaw + turn_step, glm::vec3(0.0f, 1.0f, 0.0f));
            std::memcpy(facing_transform.rotation, glm::value_ptr(rotation), sizeof(facing_transform.rotation));
            engineGameObjectUpdateTransformComponent(go_, &facing_transform);
        }
    }
}

bool project_c::Player::equip_waepon(Weapon* sword)
{
    if (!weapon_ && sword)
    {
        weapon_ = sword;
        weapon_->attach_to_game_object(go_, "handslot.r", glm::vec3(0.0f, 0.0f, 0.0f), glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        weapon_->attach_offhand_to_game_object(go_, "handslot.l");
        return true;
    }
    return false;
}

void project_c::Player::on_confirmed_attack_hit(float hit_stop_seconds)
{
    if ((state_ & States::DEAD) != 0)
    {
        return;
    }
    hit_stop_remaining_seconds_ = std::max(hit_stop_remaining_seconds_, hit_stop_seconds);
    hit_stop_just_started_ = true;
    const auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;
    if (!engineAnimationControllerSetPaused(animation_controller, true))
    {
        engineLog("Failed to pause player animation for hit-stop.\n");
    }

    const auto cameras = utils::get_active_camera_game_objects();
    if (!cameras.empty())
    {
        my_scene_->get_script<CameraScript>(cameras.front())->shake(
            HIT_CAMERA_SHAKE_INTENSITY,
            HIT_CAMERA_SHAKE_DURATION_SECONDS);
    }
}

bool project_c::Player::receive_damage(std::uint32_t damage, const glm::vec3& source_position)
{
    if (hp_ == 0 || dodge_invulnerability_remaining_seconds_ > 0.0f)
    {
        return false;
    }

    hp_ = damage >= hp_ ? 0 : hp_ - damage;
    auto typed_scene = static_cast<project_c::TestScene*>(my_scene_);
    typed_scene->ui_update_player_health(hp_);

    auto player_transform = engineGameObjectGetTransformComponent(go_);
    glm::vec3 knockback_direction(
        player_transform.position[0] - source_position.x,
        0.0f,
        player_transform.position[2] - source_position.z);
    if (glm::dot(knockback_direction, knockback_direction) <= 0.0001f)
    {
        knockback_direction = glm::make_quat(player_transform.rotation) * glm::vec3(0.0f, 0.0f, 1.0f);
    }
    knockback_velocity_ = glm::normalize(knockback_direction) * PLAYER_KNOCKBACK_SPEED;

    const auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;
    if (hp_ == 0)
    {
        // fall off the wall
        stop_traversal(animation_controller, 0.0f);
        update_traversal_marker({}, 0.0f);
        state_ |= States::DEAD;
        movement_velocity_ = glm::vec2(0.0f);
        knockback_velocity_ = glm::vec3(0.0f);
        attack_trigger_->deactivate();
        if (!engineAnimationControllerSetPaused(animation_controller, false))
        {
            engineLog("Failed to resume player animation before death.\n");
        }
        const bool combat_layer_hidden = engineAnimationControllerLayerSetWeight(animation_controller, COMBAT_LAYER_ID, 0.0f);
        const bool locomotion_stopped_looping = engineAnimationControllerLayerSetLooping(animation_controller, LOCOMOTION_LAYER_ID, false);
        assert(combat_layer_hidden && locomotion_stopped_looping);
        if (!engineAnimationControllerAnimationPlay(animation_controller, "Death_A", LOCOMOTION_LAYER_ID))
        {
            engineLog("Failed to play player death animation.\n");
        }
        typed_scene->on_player_died();
    }
    else
    {
        const bool disable_attack_interrupt = true;
        if(!disable_attack_interrupt)
        {
            if ((state_ & States::ATTACK) != 0)
            {
                state_ &= ~States::ATTACK;
                attack_phase_ = AttackPhase::eIdle;
                attack_buffered_ = false;
                attack_trigger_->deactivate();
                engineAnimationControllerLayerSetPlaybackRate(animation_controller, COMBAT_LAYER_ID, 1.0f);
                if (!engineAnimationControllerAnimationCrossFade(animation_controller, "Unarmed_Idle", COMBAT_LAYER_ID, 0.08f))
                {
                    engineLog("Failed to interrupt player attack after taking damage.\n");
                }
            }
        }

        hit_stun_remaining_seconds_ = PLAYER_HIT_STUN_SECONDS;
        hit_stop_remaining_seconds_ = std::max(hit_stop_remaining_seconds_, 0.04f);
        hit_stop_just_started_ = true;
        if (!engineAnimationControllerSetPaused(animation_controller, true))
        {
            engineLog("Failed to pause player animation after taking damage.\n");
        }
    }

    const auto cameras = utils::get_active_camera_game_objects();
    if (!cameras.empty())
    {
        my_scene_->get_script<CameraScript>(cameras.front())->shake(0.05f, 0.12f);
    }
    return true;
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
    switch (dir)
    {
    case Direction::eForward:
        return "Running_A";
    case Direction::eBackward:
        return "Walking_Backwards";
    case Direction::eLeft:
        return "Running_Strafe_Left";
    case Direction::eRight:
        return "Running_Strafe_Right";
    default:
        assert(!"Unknown move direction for player!");
    }
    return "";
}

float project_c::Player::MoveStateData::get_animation_reference_speed(Direction dir) const
{
    if (dir == Direction::eBackward)
    {
        return PLAYER_WALK_SPEED;
    }
    if (dir == Direction::eLeft || dir == Direction::eRight)
    {
        return PLAYER_RUN_SPEED;
    }
    return PLAYER_RUN_SPEED;
}

const project_c::Player::AttackStateData::Attack& project_c::Player::AttackStateData::get_attack(std::size_t combo_index) const
{
    // Hit windows match the moments when daggers are actually swinging in the animations.
    static constexpr std::array<Attack, combo_length> attacks = { {
        // right dagger slash, then left dagger chop
        { "Dualwield_Melee_Attack_Chop",  { { { 0.42f, 0.60f }, { 0.80f, 0.92f } } }, 2, 0.94f, 1.20f, 1.26f },
        // both daggers: overhead cut followed by outward sweep
        { "Dualwield_Melee_Attack_Slice", { { { 0.52f, 0.83f }, { 0.0f, 0.0f } } },   1, 0.85f, 1.10f, 1.16f },
        // finisher: double thrust, animation holds the pose for a long time so leave it early
        { "Dualwield_Melee_Attack_Stab",  { { { 0.35f, 0.50f }, { 0.0f, 0.0f } } },   1, 0.0f,  0.0f,  0.95f },
    } };
    assert(combo_index < attacks.size());
    return attacks[combo_index];
}

const char* project_c::Player::AttackStateData::get_animation_name(std::size_t combo_index) const
{
    return get_attack(combo_index).animation_name;
}

bool project_c::Player::start_attack(engine_animation_controller_t* animation_controller, std::size_t combo_index)
{
    if (combo_index >= AttackStateData::combo_length)
    {
        return false;
    }

    const auto animation_name = attack_data_.get_animation_name(combo_index);
    float duration = 0.0f;
    engineAnimationControllerAnimationGetDuration(animation_controller, animation_name, &duration);
    // Checked before the combat layer is touched: on failure the layer stays as it was (idle, or the attack which is still playing).
    // Play and CrossFade fail without changing the layer.
    const bool animation_started = duration > 0.0f && (combo_index == 0 && !combat_layer_has_pose_
        ? engineAnimationControllerAnimationPlay(animation_controller, animation_name, COMBAT_LAYER_ID)
        : engineAnimationControllerAnimationCrossFade(
            animation_controller,
            animation_name,
            COMBAT_LAYER_ID,
            combo_index == 0 ? ATTACK_EXIT_CROSS_FADE_SECONDS : ATTACK_COMBO_CROSS_FADE_SECONDS));
    if (!animation_started)
    {
        engineLog(std::format("Failed to start attack animation '{}'.\n", animation_name).c_str());
        return false;
    }

    const bool combat_layer_stopped_looping = engineAnimationControllerLayerSetLooping(animation_controller, COMBAT_LAYER_ID, false);
    const bool combat_rate_set = engineAnimationControllerLayerSetPlaybackRate(animation_controller, COMBAT_LAYER_ID, ATTACK_PLAYBACK_RATE);
    assert(combat_layer_stopped_looping && combat_rate_set);

    attack_trigger_->begin_swing();
    attack_combo_index_ = combo_index;
    attack_elapsed_ = 0.0f;
    attack_phase_ = AttackPhase::eWindup;
    attack_buffered_ = false;
    state_ |= States::ATTACK;
    combat_layer_has_pose_ = true;
    return true;
}

bool project_c::Player::start_dodge(engine_animation_controller_t* animation_controller, const glm::vec2& move_input, const glm::quat& rotation)
{
    glm::vec3 world_direction(move_input.x, 0.0f, move_input.y);
    if (glm::dot(world_direction, world_direction) <= 0.0001f)
    {
        world_direction = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    }
    world_direction = glm::normalize(world_direction);

    const glm::vec3 local_direction = glm::inverse(rotation) * world_direction;
    const char* animation_name = "Dodge_Forward";
    if (std::abs(local_direction.z) >= std::abs(local_direction.x))
    {
        animation_name = local_direction.z >= 0.0f ? "Dodge_Forward" : "Dodge_Backward";
    }
    else
    {
        animation_name = local_direction.x >= 0.0f ? "Dodge_Right" : "Dodge_Left";
    }

    float duration = 0.0f;
    engineAnimationControllerAnimationGetDuration(animation_controller, animation_name, &duration);
    if (duration <= 0.0f)
    {
        engineLog(std::format("Dodge animation '{}' has no valid duration.\n", animation_name).c_str());
        return false;
    }

    const bool locomotion_stopped_looping = engineAnimationControllerLayerSetLooping(animation_controller, LOCOMOTION_LAYER_ID, false);
    const bool locomotion_rate_reset = engineAnimationControllerLayerSetPlaybackRate(animation_controller, LOCOMOTION_LAYER_ID, 1.0f);
    const bool combat_layer_hidden = engineAnimationControllerLayerSetWeight(animation_controller, COMBAT_LAYER_ID, 0.0f);
    if (!locomotion_stopped_looping || !locomotion_rate_reset || !combat_layer_hidden
        || !engineAnimationControllerAnimationCrossFade(animation_controller, animation_name, LOCOMOTION_LAYER_ID, 0.05f))
    {
        engineLog("Failed to start player dodge.\n");
        engineAnimationControllerLayerSetLooping(animation_controller, LOCOMOTION_LAYER_ID, true);
        engineAnimationControllerLayerSetWeight(animation_controller, COMBAT_LAYER_ID, 1.0f);
        return false;
    }

    dodge_direction_ = world_direction;
    dodge_remaining_seconds_ = duration;
    dodge_invulnerability_remaining_seconds_ = std::min(DODGE_INVULNERABILITY_SECONDS, duration);
    dodge_cooldown_remaining_seconds_ = DODGE_COOLDOWN_SECONDS;
    movement_velocity_ = glm::vec2(0.0f);
    return true;
}

void project_c::Player::set_kinematic(bool kinematic)
{
    // Rigid body removed -> collider becomes static and is moved only by the transform (no gravity).
    // Rigid body added back -> fresh dynamic body, so no velocity accumulated while traversing is applied.
    if (kinematic && engineGameObjectHasRigidBodyComponent(go_))
    {
        engineGameObjectRemoveRigidBodyComponent(go_);
    }
    else if (!kinematic && !engineGameObjectHasRigidBodyComponent(go_))
    {
        auto rbc = engineGameObjectAddRigidBodyComponent(go_);
        rbc.mass = PLAYER_MASS;
        engineGameObjectUpdateRigidBodyComponent(go_, &rbc);
    }
}

void project_c::Player::set_position_and_yaw(const glm::vec3& position, float yaw)
{
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] = position.x;
    tc.position[1] = position.y;
    tc.position[2] = position.z;
    const glm::quat rotation = glm::angleAxis(yaw, WORLD_UP);
    std::memcpy(tc.rotation, glm::value_ptr(rotation), sizeof(tc.rotation));
    engineGameObjectUpdateTransformComponent(go_, &tc);
}

project_c::Player::ProbeHit project_c::Player::probe(const glm::vec3& from, const glm::vec3& to) const
{
    std::array<engine_game_object_t, 3> ignore_list = { go_, attack_trigger_->get_game_object(), ENGINE_INVALID_GAME_OBJECT_ID };
    if (weapon_)
    {
        ignore_list[2] = weapon_->get_game_object();
    }

    // note: engine ray "direction" is the end point of the ray
    engine_ray_t ray{};
    ray.origin = { from.x, from.y, from.z };
    ray.direction = { to.x, to.y, to.z };
    const auto hit_info = enginePhysicsRayCast(ignore_list.data(), ignore_list.size(), &ray, glm::distance(from, to));

    ProbeHit ret{};
    if (hit_info.go == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return ret;
    }
    ret.hit = true;
    ret.go = hit_info.go;
    ret.position = glm::vec3(hit_info.position.x, hit_info.position.y, hit_info.position.z);
    ret.normal = glm::vec3(hit_info.normal.x, hit_info.normal.y, hit_info.normal.z);
    if (const auto* part = my_scene_->get_script<BuildingPart>(hit_info.go))
    {
        ret.building = true;
        ret.climbable = part->is_climbable(ret.normal);
    }
    return ret;
}

project_c::Player::TraversalPlan project_c::Player::find_traversal(const glm::vec3& position, const glm::vec3& direction) const
{
    const glm::vec3 knee_from = position + WORLD_UP * PROBE_KNEE_HEIGHT;
    const glm::vec3 chest_from = position + WORLD_UP * PROBE_CHEST_HEIGHT;
    const auto knee_hit = probe(knee_from, knee_from + direction * TRAVERSAL_DETECT_RANGE);
    const auto chest_hit = probe(chest_from, chest_from + direction * TRAVERSAL_DETECT_RANGE);
    const auto distance_to = [&](const ProbeHit& h) { return glm::dot(h.position - position, direction); };
    // vertical face of a building, approached head-on (walking along a wall does not trigger anything)
    const auto is_wall_ahead = [&](const ProbeHit& h)
        {
            return h.building
                && std::abs(h.normal.y) < 0.3f
                && glm::dot(direction, -flatten(h.normal)) >= TRAVERSAL_MIN_APPROACH_DOT;
        };

    // something low in front, free space above -> vault over (window sill, low wall)
    if (is_wall_ahead(knee_hit) && (!chest_hit.hit || distance_to(chest_hit) > distance_to(knee_hit) + 0.1f))
    {
        auto plan = plan_vault(position, knee_hit);
        plan.distance = distance_to(knee_hit);
        return plan;
    }

    // climbable wall in front
    if (is_wall_ahead(chest_hit) && chest_hit.climbable)
    {
        TraversalPlan plan{};
        plan.type = TraversalPlan::Type::eClimb;
        plan.distance = distance_to(chest_hit);
        plan.wall_normal = flatten(chest_hit.normal);
        plan.climb_position = chest_hit.position + plan.wall_normal * CLIMB_WALL_OFFSET;
        plan.climb_position.y = position.y + 0.05f;
        // where the hands grab the wall
        plan.marker = chest_hit.position + plan.wall_normal * TRAVERSAL_MARKER_SIZE + WORLD_UP * (PROBE_HEAD_HEIGHT - PROBE_CHEST_HEIGHT);
        return plan;
    }

    // walking toward the edge of a roof
    return plan_climb_down(position, direction);
}

project_c::Player::TraversalPlan project_c::Player::find_nearest_traversal(const glm::vec3& position) const
{
    // 8 directions, 45 degrees apart, so every wall is approached within TRAVERSAL_MIN_APPROACH_DOT by one of them
    constexpr int directions_count = 8;
    TraversalPlan nearest{};
    for (int i = 0; i < directions_count; i++)
    {
        const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(directions_count);
        const auto plan = find_traversal(position, glm::vec3(std::sin(angle), 0.0f, std::cos(angle)));
        if (plan.type != TraversalPlan::Type::eNone && (nearest.type == TraversalPlan::Type::eNone || plan.distance < nearest.distance))
        {
            nearest = plan;
        }
    }
    return nearest;
}

project_c::Player::TraversalPlan project_c::Player::plan_vault(const glm::vec3& position, const ProbeHit& low_hit) const
{
    const glm::vec3 wall_normal = flatten(low_hit.normal);
    const glm::vec3 forward = -wall_normal;
    const glm::vec3 tangent = glm::normalize(glm::cross(WORLD_UP, wall_normal));

    // obstacle depth: cast back toward the player from the far side
    const auto far_hit = probe(low_hit.position + forward * VAULT_MAX_DEPTH, low_hit.position);
    if (!far_hit.building)
    {
        return {};
    }
    const float depth = glm::dot(far_hit.position - low_hit.position, forward);

    // top of the obstacle
    const glm::vec3 middle = low_hit.position + forward * (depth * 0.5f);
    const auto top_hit = probe(glm::vec3(middle.x, position.y + VAULT_MAX_HEIGHT, middle.z), glm::vec3(middle.x, position.y, middle.z));
    if (!top_hit.hit || top_hit.normal.y < 0.7f)
    {
        return {};
    }
    const float top_y = top_hit.position.y;

    // body has to fit above the obstacle (i.e. window opening height)
    if (probe(top_hit.position + WORLD_UP * 0.02f, top_hit.position + WORLD_UP * PLAYER_HEIGHT).hit)
    {
        return {};
    }

    // align with the window opening
    float lateral_offset = 0.0f;
    if (const auto* part = my_scene_->get_script<BuildingPart>(low_hit.go))
    {
        if (const auto alignment_point = part->get_alignment_point())
        {
            const float offset = glm::dot(alignment_point.value() - position, tangent);
            if (std::abs(offset) <= VAULT_MAX_ALIGNMENT)
            {
                lateral_offset = offset;
            }
        }
    }

    // landing on the other side
    const glm::vec3 landing = low_hit.position + forward * (depth + VAULT_LANDING_DISTANCE) + tangent * lateral_offset;
    const auto ground_hit = probe(glm::vec3(landing.x, top_y + 0.3f, landing.z), glm::vec3(landing.x, top_y - 3.0f, landing.z));
    if (!ground_hit.hit)
    {
        return {};
    }

    TraversalPlan plan{};
    plan.type = TraversalPlan::Type::eVault;
    plan.wall_normal = wall_normal;
    plan.move.p0 = position;
    plan.move.p1 = glm::vec3(middle.x, top_y + VAULT_CLEARANCE, middle.z) + tangent * lateral_offset;
    plan.move.p2 = glm::vec3(landing.x, ground_hit.position.y + 0.01f, landing.z);
    plan.move.duration = VAULT_DURATION;
    plan.move.curved = true;
    plan.move.yaw = yaw_from_direction(forward);
    // on top of the sill
    plan.marker = glm::vec3(middle.x, top_y + TRAVERSAL_MARKER_SIZE, middle.z) + tangent * lateral_offset;
    return plan;
}

project_c::Player::TraversalPlan project_c::Player::plan_climb_down(const glm::vec3& position, const glm::vec3& direction) const
{
    // sample a few points ahead, first one without floor below is the edge
    for (const float edge_distance : { CLIMB_DOWN_EDGE_DISTANCE, CLIMB_DOWN_EDGE_DISTANCE + 0.4f, CLIMB_DOWN_EDGE_DISTANCE + 0.8f })
    {
        const glm::vec3 edge_point = position + direction * edge_distance;
        if (probe(edge_point + WORLD_UP * 0.2f, edge_point - WORLD_UP * 0.5f).hit)
        {
            continue;
        }

        // look back at the wall below the edge
        const glm::vec3 below = -WORLD_UP * (PLAYER_HEIGHT * 0.7f);
        const auto wall_hit = probe(position + direction * (edge_distance + CLIMB_DOWN_EDGE_DISTANCE) + below, position + below);
        if (!wall_hit.climbable)
        {
            return {};
        }
        const glm::vec3 wall_normal = flatten(wall_hit.normal);
        if (glm::dot(wall_normal, direction) < TRAVERSAL_MIN_APPROACH_DOT)
        {
            return {};
        }

        glm::vec3 hang_position = wall_hit.position + wall_normal * CLIMB_WALL_OFFSET;
        hang_position.y = position.y - CLIMB_DOWN_HANG_DEPTH;

        TraversalPlan plan{};
        plan.type = TraversalPlan::Type::eClimbDown;
        plan.distance = edge_distance - CLIMB_DOWN_EDGE_DISTANCE;
        plan.wall_normal = wall_normal;
        plan.move.p0 = position;
        plan.move.p1 = glm::vec3(hang_position.x, position.y + 0.1f, hang_position.z);
        plan.move.p2 = hang_position;
        plan.move.duration = CLIMB_DOWN_DURATION;
        plan.move.split = 0.4f;
        plan.move.yaw = yaw_from_direction(-wall_normal);
        plan.move.climb_after = true;
        // on the edge
        plan.marker = glm::vec3(wall_hit.position.x, position.y + TRAVERSAL_MARKER_SIZE, wall_hit.position.z) - wall_normal * TRAVERSAL_MARKER_SIZE;
        return plan;
    }
    return {};
}

void project_c::Player::execute_traversal(engine_animation_controller_t* animation_controller, const TraversalPlan& plan)
{
    switch (plan.type)
    {
    case TraversalPlan::Type::eClimb:
        start_climbing(animation_controller, plan.wall_normal, plan.climb_position);
        break;
    case TraversalPlan::Type::eVault:
        start_traversal_move(animation_controller, plan.move, VAULT_ANIMATION);
        break;
    case TraversalPlan::Type::eClimbDown:
        climb_wall_normal_ = plan.wall_normal;
        start_traversal_move(animation_controller, plan.move, CLIMB_ANIMATION);
        break;
    default:
        break;
    }
}

bool project_c::Player::try_start_mantle(engine_animation_controller_t* animation_controller, const glm::vec3& position)
{
    const glm::vec3 into_wall = -climb_wall_normal_;
    const glm::vec3 ledge_xz = position + into_wall * (CLIMB_WALL_OFFSET + MANTLE_LEDGE_DEPTH);
    const auto ledge_hit = probe(
        glm::vec3(ledge_xz.x, position.y + MANTLE_MAX_LEDGE_HEIGHT, ledge_xz.z),
        glm::vec3(ledge_xz.x, position.y + PROBE_KNEE_HEIGHT, ledge_xz.z));
    if (!ledge_hit.building || ledge_hit.normal.y < 0.7f)
    {
        return false;
    }

    // standing room above the ledge
    if (probe(ledge_hit.position + WORLD_UP * 0.02f, ledge_hit.position + WORLD_UP * PLAYER_HEIGHT).hit)
    {
        return false;
    }

    // nothing above the player on the way up (i.e. ceiling when climbing on the inner side of a wall)
    const float ledge_y = ledge_hit.position.y;
    if (probe(position + WORLD_UP * PROBE_CHEST_HEIGHT, glm::vec3(position.x, ledge_y + PLAYER_HEIGHT, position.z)).hit)
    {
        return false;
    }

    TraversalMove move{};
    move.p0 = position;
    move.p1 = glm::vec3(position.x, ledge_y + 0.05f, position.z);
    move.p2 = position + into_wall * (CLIMB_WALL_OFFSET + MANTLE_LEDGE_DEPTH + 0.15f);
    move.p2.y = ledge_y + 0.02f;
    move.duration = MANTLE_DURATION;
    move.split = 0.55f;
    move.yaw = yaw_from_direction(into_wall);
    start_traversal_move(animation_controller, move, MANTLE_ANIMATION);
    return true;
}

void project_c::Player::start_climbing(engine_animation_controller_t* animation_controller, const glm::vec3& wall_normal, const glm::vec3& position)
{
    if (traversal_state_ == TraversalState::eNone)
    {
        set_kinematic(true);
        engineAnimationControllerLayerSetWeight(animation_controller, COMBAT_LAYER_ID, 0.0f);
    }
    traversal_state_ = TraversalState::eClimbing;
    climb_wall_normal_ = wall_normal;
    movement_velocity_ = glm::vec2(0.0f);
    set_position_and_yaw(position, yaw_from_direction(-wall_normal));

    engineAnimationControllerLayerSetLooping(animation_controller, LOCOMOTION_LAYER_ID, true);
    if (!engineAnimationControllerIsAnimationPlayingOnLayer(animation_controller, CLIMB_ANIMATION, LOCOMOTION_LAYER_ID))
    {
        engineAnimationControllerAnimationCrossFade(animation_controller, CLIMB_ANIMATION, LOCOMOTION_LAYER_ID, 0.08f);
    }
}

void project_c::Player::start_traversal_move(engine_animation_controller_t* animation_controller, const TraversalMove& move, const char* animation_name)
{
    if (traversal_state_ == TraversalState::eNone)
    {
        set_kinematic(true);
        engineAnimationControllerLayerSetWeight(animation_controller, COMBAT_LAYER_ID, 0.0f);
    }
    traversal_state_ = TraversalState::eScriptedMove;
    traversal_move_ = move;
    traversal_move_.elapsed = 0.0f;
    movement_velocity_ = glm::vec2(0.0f);

    // stretch the clip over the whole move
    float duration = 0.0f;
    engineAnimationControllerAnimationGetDuration(animation_controller, animation_name, &duration);
    const float playback_rate = duration > 0.0f ? std::clamp(duration / move.duration, 0.25f, 4.0f) : 1.0f;
    engineAnimationControllerLayerSetPlaybackRate(animation_controller, LOCOMOTION_LAYER_ID, playback_rate);
    if (!engineAnimationControllerIsAnimationPlayingOnLayer(animation_controller, animation_name, LOCOMOTION_LAYER_ID))
    {
        engineAnimationControllerLayerSetLooping(animation_controller, LOCOMOTION_LAYER_ID, false);
        engineAnimationControllerAnimationCrossFade(animation_controller, animation_name, LOCOMOTION_LAYER_ID, 0.06f);
    }
}

void project_c::Player::stop_traversal(engine_animation_controller_t* animation_controller, float regrab_cooldown)
{
    if (traversal_state_ == TraversalState::eNone)
    {
        return;
    }
    traversal_state_ = TraversalState::eNone;
    // player usually still holds the direction toward the wall, do not grab it again right away
    traversal_regrab_cooldown_ = regrab_cooldown;
    set_kinematic(false);
    // locomotion code picks idle/run animation in the next update
    engineAnimationControllerLayerSetLooping(animation_controller, LOCOMOTION_LAYER_ID, true);
    engineAnimationControllerLayerSetPlaybackRate(animation_controller, LOCOMOTION_LAYER_ID, 1.0f);
    engineAnimationControllerLayerSetWeight(animation_controller, COMBAT_LAYER_ID, 1.0f);
}

void project_c::Player::update_traversal(engine_animation_controller_t* animation_controller, float delta_seconds, bool drop_pressed)
{
    const auto tc = engineGameObjectGetTransformComponent(go_);
    glm::vec3 position(tc.position[0], tc.position[1], tc.position[2]);

    if (traversal_state_ == TraversalState::eScriptedMove)
    {
        auto& move = traversal_move_;
        move.elapsed = std::min(move.elapsed + delta_seconds, move.duration);
        const float t = move.duration > 0.0f ? move.elapsed / move.duration : 1.0f;
        if (move.curved)
        {
            // quadratic bezier passing through p1 at t = 0.5
            const glm::vec3 control = 2.0f * move.p1 - 0.5f * (move.p0 + move.p2);
            const float u = 1.0f - t;
            position = u * u * move.p0 + 2.0f * u * t * control + t * t * move.p2;
        }
        else if (t < move.split)
        {
            position = glm::mix(move.p0, move.p1, glm::smoothstep(0.0f, 1.0f, t / move.split));
        }
        else
        {
            position = glm::mix(move.p1, move.p2, glm::smoothstep(0.0f, 1.0f, (t - move.split) / (1.0f - move.split)));
        }
        set_position_and_yaw(position, move.yaw);

        if (move.elapsed >= move.duration)
        {
            if (move.climb_after)
            {
                start_climbing(animation_controller, climb_wall_normal_, move.p2);
            }
            else
            {
                stop_traversal(animation_controller, TRAVERSAL_REGRAB_COOLDOWN);
            }
        }
        return;
    }

    // climbing
    const glm::vec3 wall_normal = climb_wall_normal_;
    const glm::vec3 tangent = glm::normalize(glm::cross(WORLD_UP, wall_normal));  // character's right while facing the wall
    if (drop_pressed)
    {
        set_position_and_yaw(position + wall_normal * CLIMB_DROP_PUSH, yaw_from_direction(-wall_normal));
        stop_traversal(animation_controller, TRAVERSAL_DROP_REGRAB_COOLDOWN);
        return;
    }

    const auto wall_probe = [&](const glm::vec3& feet, float height)
        {
            const glm::vec3 from = feet + WORLD_UP * height;
            return probe(from, from - wall_normal * (CLIMB_WALL_OFFSET + CLIMB_PROBE_DEPTH));
        };
    const auto touches_wall = [&](const glm::vec3& feet)
        {
            for (const float height : { PROBE_KNEE_HEIGHT, PROBE_CHEST_HEIGHT, PROBE_HEAD_HEIGHT })
            {
                if (wall_probe(feet, height).climbable)
                {
                    return true;
                }
            }
            return false;
        };

    // Same WASD directions as on the ground: pushing toward the wall climbs up, away from it climbs down,
    // along the wall shimmies sideways. I.e. wall on the right side of the screen -> holding D climbs.
    glm::vec3 input(0.0f);
    if (engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_A)) input.x -= 1.0f;
    if (engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_D)) input.x += 1.0f;
    if (engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_W)) input.z -= 1.0f;
    if (engineKeyboardIsButtonDown(ENGINE_KEYBOARD_KEY_S)) input.z += 1.0f;
    if (glm::length(input) > 1.0f)
    {
        input = glm::normalize(input);
    }
    float vertical_input = glm::dot(input, -wall_normal);
    float lateral_input = glm::dot(input, tangent);
    if (std::abs(vertical_input) < CLIMB_INPUT_DEADZONE) vertical_input = 0.0f;
    if (std::abs(lateral_input) < CLIMB_INPUT_DEADZONE) lateral_input = 0.0f;

    if (lateral_input != 0.0f)
    {
        const float lateral_sign = lateral_input > 0.0f ? 1.0f : -1.0f;
        const glm::vec3 candidate = position + tangent * (lateral_input * CLIMB_SPEED_LATERAL * delta_seconds);
        // leading side of the body has to stay on the wall, so the player does not hang over a corner
        if (touches_wall(candidate + tangent * (lateral_sign * PLAYER_HALF_WIDTH)))
        {
            position = candidate;
        }
    }

    if (vertical_input > 0.0f)
    {
        const glm::vec3 candidate = position + WORLD_UP * (vertical_input * CLIMB_SPEED_VERTICAL * delta_seconds);
        if (!wall_probe(candidate, PROBE_HEAD_HEIGHT).climbable)
        {
            // head above the wall: pull up onto the ledge if there is one
            if (try_start_mantle(animation_controller, position))
            {
                return;
            }
            if (touches_wall(candidate))
            {
                position = candidate;
            }
        }
        else
        {
            position = candidate;
        }
    }
    else if (vertical_input < 0.0f)
    {
        const glm::vec3 candidate = position + WORLD_UP * (vertical_input * CLIMB_SPEED_VERTICAL * delta_seconds);
        const auto ground_hit = probe(position + WORLD_UP * 0.05f, candidate - WORLD_UP * 0.02f);
        if (ground_hit.hit && ground_hit.normal.y > 0.7f)
        {
            // reached the ground
            set_position_and_yaw(glm::vec3(position.x, ground_hit.position.y + 0.01f, position.z), yaw_from_direction(-wall_normal));
            stop_traversal(animation_controller, TRAVERSAL_REGRAB_COOLDOWN);
            return;
        }
        if (touches_wall(candidate))
        {
            position = candidate;
        }
    }

    // keep constant distance from the wall surface
    bool on_wall = false;
    for (const float height : { PROBE_CHEST_HEIGHT, PROBE_KNEE_HEIGHT, PROBE_HEAD_HEIGHT })
    {
        const auto hit = wall_probe(position, height);
        if (hit.climbable)
        {
            position += wall_normal * (CLIMB_WALL_OFFSET - glm::dot(position - hit.position, wall_normal));
            on_wall = true;
            break;
        }
    }
    set_position_and_yaw(position, yaw_from_direction(-wall_normal));
    if (!on_wall)
    {
        // lost the wall (i.e. it was removed), fall
        stop_traversal(animation_controller, TRAVERSAL_REGRAB_COOLDOWN);
        return;
    }

    const bool moving = vertical_input != 0.0f || lateral_input != 0.0f;
    engineAnimationControllerLayerSetPlaybackRate(animation_controller, LOCOMOTION_LAYER_ID, moving ? CLIMB_ANIMATION_RATE_MOVING : CLIMB_ANIMATION_RATE_IDLE);
}

void project_c::Player::create_traversal_marker()
{
    auto* app = static_cast<AppProjectC*>(my_scene_->get_app());
    if (!app->is_prefab_available(PREFAB_TYPE_TRAVERSAL_MARKER))
    {
        return;
    }
    traversal_marker_ = app->instantiate_prefab(PREFAB_TYPE_TRAVERSAL_MARKER, my_scene_).go;
    if (traversal_marker_ == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return;
    }

    auto tc = engineGameObjectGetTransformComponent(traversal_marker_);
    tc.scale[0] = TRAVERSAL_MARKER_SIZE;
    tc.scale[1] = TRAVERSAL_MARKER_SIZE;
    tc.scale[2] = TRAVERSAL_MARKER_SIZE;
    engineGameObjectUpdateTransformComponent(traversal_marker_, &tc);

    // flat color, set per traversal type
    auto mat = engineGameObjectHasMaterialComponent(traversal_marker_)
        ? engineGameObjectGetMaterialComponent(traversal_marker_)
        : engineGameObjectAddMaterialComponent(traversal_marker_);
    mat.data.pong.diffuse_texture = ENGINE_INVALID_OBJECT_HANDLE;
    mat.data.pong.specular_texture = ENGINE_INVALID_OBJECT_HANDLE;
    engineGameObjectUpdateMaterialComponent(traversal_marker_, &mat);

    auto mc = engineGameObjectGetMeshComponent(traversal_marker_);
    mc.disable = true;
    engineGameObjectUpdateMeshComponent(traversal_marker_, &mc);
}

void project_c::Player::update_traversal_marker(const TraversalPlan& plan, float delta_seconds)
{
    if (traversal_marker_ == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return;
    }

    const bool visible = plan.type != TraversalPlan::Type::eNone;
    auto mc = engineGameObjectGetMeshComponent(traversal_marker_);
    if (mc.disable == visible)
    {
        mc.disable = !visible;
        engineGameObjectUpdateMeshComponent(traversal_marker_, &mc);
    }
    if (!visible)
    {
        traversal_marker_type_ = TraversalPlan::Type::eNone;
        traversal_marker_time_ = 0.0f;
        return;
    }

    if (traversal_marker_type_ != plan.type)
    {
        traversal_marker_type_ = plan.type;
        // climb - yellow, vault - green, climb down - cyan
        std::array<float, 4> color = { 1.0f, 0.85f, 0.1f, 1.0f };
        if (plan.type == TraversalPlan::Type::eVault)
        {
            color = { 0.2f, 1.0f, 0.3f, 1.0f };
        }
        else if (plan.type == TraversalPlan::Type::eClimbDown)
        {
            color = { 0.2f, 0.8f, 1.0f, 1.0f };
        }
        auto mat = engineGameObjectGetMaterialComponent(traversal_marker_);
        set_c_array(mat.data.pong.diffuse_color, color);
        engineGameObjectUpdateMaterialComponent(traversal_marker_, &mat);
    }

    // pulse
    traversal_marker_time_ += delta_seconds;
    const float scale = TRAVERSAL_MARKER_SIZE * (1.0f + 0.3f * std::sin(traversal_marker_time_ * 10.0f));
    auto tc = engineGameObjectGetTransformComponent(traversal_marker_);
    tc.position[0] = plan.marker.x;
    tc.position[1] = plan.marker.y - scale;  // mesh pivot is at the bottom of the cube
    tc.position[2] = plan.marker.z;
    tc.scale[0] = scale;
    tc.scale[1] = scale;
    tc.scale[2] = scale;
    engineGameObjectUpdateTransformComponent(traversal_marker_, &tc);
}
