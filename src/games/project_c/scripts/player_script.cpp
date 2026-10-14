#include "player_script.h"
#include "scripts_utils.h"
#include "enemy_script.h"
#include "enviorment_script.h"
#include "interactable_script.h"
#include "camera_script.h"
#include "../scenes/scene_test.h"

#include "../app.h"
#include "iscene.h"

#include <glm/glm.hpp>
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
    mc.geometry = engineGeometryGetByName("Cylinder.008");
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
    offhand_mc.geometry = engineGeometryGetByName("Cylinder.002");
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
    auto rbc = engineGameObjectAddRigidBodyComponent(go_);
    rbc.mass = 100000.0f;
    engineGameObjectUpdateRigidBodyComponent(go_, &rbc);

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