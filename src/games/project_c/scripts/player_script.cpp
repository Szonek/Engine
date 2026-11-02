#include "player_script.h"
#include "scripts_utils.h"
#include "enemy_script.h"
#include "enviorment_script.h"
#include "interactable_script.h"
#include "camera_script.h"

#include "game_components.h"

#include "../app.h"
#include "iscene.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace
{
// the scale of the player's model; its children (attack trigger, weapon) inherit it, so their colliders are in the
// model's units
constexpr float PLAYER_SCALE = project_c::Player::MODEL_SCALE;
// the speeds the locomotion animations were made for: their playback rate follows the actual speed
constexpr float PLAYER_WALK_ANIMATION_SPEED = 1.875f;
constexpr float PLAYER_RUN_ANIMATION_SPEED = 2.5f;
constexpr float PLAYER_IDLE_SPEED_THRESHOLD = 0.05f;
// the player falls (airborne) when the character controller hasn't stood it on the ground for this long, so a frame
// on the edge of a step isn't a fall
constexpr float FALL_DELAY_SECONDS = 0.1f;
constexpr const char* LANDING_ANIMATION = "Jump_Land";

void attack_window_open_callback(const engine_animation_event_info_t* info, void* user_data)
{
    assert(info);
    assert(user_data);
    static_cast<project_c::Player*>(user_data)->set_attack_window_active(true);
}

void attack_window_close_callback(const engine_animation_event_info_t* info, void* user_data)
{
    assert(info);
    assert(user_data);
    static_cast<project_c::Player*>(user_data)->set_attack_window_active(false);
}
} // namespace anonymous

project_c::Weapon::Weapon(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go)
{
}

void project_c::Weapon::add_components(engine_game_object_t go, const AppProjectC& app)
{
    const auto dagger_texture = engineTextured2DGetByName(app.model_asset_id(PREFAB_TYPE_ROGUE, "rogue_texture").c_str());

    auto nc = engineGameObjectAddNameComponent(go);
    std::strncpy(nc.name, "weapon-daggers", std::size(nc.name) - 1);
    engineGameObjectUpdateNameComponent(go, &nc);

    // transform
    auto tc = engineGameObjectAddTransformComponent(go);
    engineGameObjectUpdateTransformComponent(go, &tc);

    // mesh
    auto mc = engineGameObjectAddMeshComponent(go);
    mc.geometry = engineGeometryGetByName(app.model_asset_id(PREFAB_TYPE_ROGUE, "Cylinder.008").c_str());
    assert(mc.geometry != ENGINE_INVALID_OBJECT_HANDLE);
    engineGameObjectUpdateMeshComponent(go, &mc);

    // material
    auto matc = engineGameObjectAddMaterialComponent(go);
    set_c_array(matc.diffuse_color, std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f});
    matc.shininess = 32.0f;
    matc.diffuse_texture = dagger_texture;
    engineGameObjectUpdateMaterialComponent(go, &matc);

    // physics
    auto cc = engineGameObjectAddColliderComponent(go);
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
    engineGameObjectUpdateColliderComponent(go, &cc);
}

void project_c::Weapon::add_lying_components(engine_game_object_t go, const glm::vec3& position)
{
    // a dynamic body, so it falls; its collider stops it on the ground
    if (!engineGameObjectHasRigidBodyComponent(go))
    {
        engineGameObjectAddRigidBodyComponent(go);
    }
    auto rbc = engineGameObjectGetRigidBodyComponent(go);
    rbc.mass = 1000.0f;
    engineGameObjectUpdateRigidBodyComponent(go, &rbc);

    auto cc = engineGameObjectGetColliderComponent(go);
    cc.is_trigger = false;
    engineGameObjectUpdateColliderComponent(go, &cc);

    // dropped from above the spot
    auto tc = engineGameObjectGetTransformComponent(go);
    tc.position[0] = position.x;
    tc.position[1] = position.y + 2.0f;
    tc.position[2] = position.z;
    engineGameObjectUpdateTransformComponent(go, &tc);
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
    jac.skin = parent;
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

void project_c::Weapon::on_start()
{
    // the second dagger is runtime-only: the weapon creates it whether it was placed by the level or spawned
    offhand_go_ = engineGameObjectCreateTransient();
    auto offhand_nc = engineGameObjectAddNameComponent(offhand_go_);
    std::strncpy(offhand_nc.name, "weapon-daggers-offhand", std::size(offhand_nc.name));
    engineGameObjectUpdateNameComponent(offhand_go_, &offhand_nc);

    auto offhand_tc = engineGameObjectAddTransformComponent(offhand_go_);
    engineGameObjectUpdateTransformComponent(offhand_go_, &offhand_tc);

    auto offhand_mc = engineGameObjectAddMeshComponent(offhand_go_);
    offhand_mc.geometry = engineGeometryGetByName(static_cast<const AppProjectC*>(my_scene_->get_app())->model_asset_id(PREFAB_TYPE_ROGUE, "Cylinder.002").c_str());
    assert(offhand_mc.geometry != ENGINE_INVALID_OBJECT_HANDLE);
    engineGameObjectUpdateMeshComponent(offhand_go_, &offhand_mc);

    auto offhand_matc = engineGameObjectAddMaterialComponent(offhand_go_);
    offhand_matc = engineGameObjectGetMaterialComponent(go_);
    engineGameObjectUpdateMaterialComponent(offhand_go_, &offhand_matc);

    place_offhand();
}

void project_c::Weapon::attach_offhand_to_game_object(engine_game_object_t parent, std::string_view joint_name)
{
    offhand_holder_ = OffhandHolder{ parent, std::string(joint_name) };
    place_offhand();
}

void project_c::Weapon::attach_offhand_to_main()
{
    offhand_holder_.reset();
    place_offhand();
}

void project_c::Weapon::place_offhand()
{
    // before on_start there is no offhand dagger yet: on_start places it
    if (offhand_go_ == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return;
    }
    if (engineGameObjectHasJointAttachmentComponent(offhand_go_))
    {
        engineGameObjectRemoveJointAttachmentComponent(offhand_go_);
    }
    if (engineGameObjectHasParentComponent(offhand_go_))
    {
        engineGameObjectRemoveParentComponent(offhand_go_);
    }
    auto pc = engineGameObjectAddParentComponent(offhand_go_);
    pc.parent = offhand_holder_ ? offhand_holder_->parent : go_;
    engineGameObjectUpdateParentComponent(offhand_go_, &pc);

    auto tc = engineGameObjectGetTransformComponent(offhand_go_);
    if (offhand_holder_)
    {
        auto jac = engineGameObjectAddJointAttachmentComponent(offhand_go_);
        jac.skin = offhand_holder_->parent;
        engineStringSet(jac.joint_name, offhand_holder_->joint_name.c_str());
        engineGameObjectUpdateJointAttachmentComponent(offhand_go_, &jac);
        set_c_array(tc.position, std::array<float, 3>{ 0.0f, 0.0f, 0.0f });
    }
    else
    {
        // lay it next to the main dagger
        set_c_array(tc.position, std::array<float, 3>{ 0.3f, 0.0f, 0.0f });
    }
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

    // keeps the size it had in its holder's hand
    if (engineGameObjectHasParentComponent(go_))
    {
        auto tc = engineGameObjectGetTransformComponent(go_);
        const auto parent_tc = engineGameObjectGetTransformComponent(engineGameObjectGetParentComponent(go_).parent);
        tc.scale[0] = parent_tc.scale[0];
        tc.scale[1] = parent_tc.scale[1];
        tc.scale[2] = parent_tc.scale[2];
        engineGameObjectUpdateTransformComponent(go_, &tc);
    }
    add_lying_components(go_, position);

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

bool project_c::Weapon::is_on_ground() const
{
    return !engineGameObjectHasParentComponent(go_);
}

project_c::AttackTrigger::AttackTrigger(engine::IScene* my_scene, engine_game_object_t go, engine_game_object_t player)
    : BaseNode(my_scene, go, "attack-trigger")
    , player_go_(player)
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
    // a box 0.6 m in front of the player, in the player's (scaled) space
    cc_child.transform[1] = 0.21f / PLAYER_SCALE;
    cc_child.transform[2] = 0.6f / PLAYER_SCALE;
    cc_child.type = ENGINE_COLLIDER_TYPE_BOX;
    set_c_array(cc_child.collider.box.size, std::array<float, 3>{ 0.3f / PLAYER_SCALE, 0.05f / PLAYER_SCALE, 0.3f / PLAYER_SCALE });
    engineGameObjectUpdateColliderComponent(go_, &cc);

    // follows the player
    auto pc = engineGameObjectAddParentComponent(go_);
    pc.parent = player_go_;
    engineGameObjectUpdateParentComponent(go_, &pc);
}

void project_c::AttackTrigger::on_collision(const collision_t& info)
{
    auto* player = my_scene_->get_script<Player>(player_go_);
    if (player && is_active_ && !hit_enemies_.contains(info.other))
    {
        if (auto* enemy = my_scene_->get_script<Enemy>(info.other))
        {
            hit_enemies_.insert(info.other);
            const auto t = player->tuning();
            enemy->receive_attack_hit(t.attack_damage, t.hit_stop_seconds);
            player->on_confirmed_attack_hit(t.hit_stop_seconds);
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

project_c::Player::Player(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go)
    , state_(States::IDLE)
    , traversal_(my_scene, go, LOCOMOTION_LAYER_ID)
{
    // the model's own weapons aren't used: the player equips a Weapon
    constexpr std::array<const char*, 5> unused_model_parts = { "Knife", "Knife_Offhand", "1H_Crossbow", "2H_Crossbow", "Throwable" };
    for (const auto child : utils::get_children(go_))
    {
        const auto name = engineGameObjectHasNameComponent(child) ? engineGameObjectGetNameComponent(child) : engine_name_component_t{};
        if (std::ranges::any_of(unused_model_parts, [&name](const char* part) { return std::strcmp(name.name, part) == 0; }))
        {
            engineGameObjectDestroy(child);
        }
    }

    // animation layers: locomotion on the whole body, combat on the upper body only (the legs keep walking while
    // attacking). The joint weights apply in order: a little of the pelvis, none of the legs, all of the spine up.
    {
        auto ac = engineGameObjectGetAnimationControllerComponent(go_);
        ac.layers_count = 2;
        auto& locomotion = ac.layers[LOCOMOTION_LAYER_ID];
        locomotion = {};
        std::strncpy(locomotion.name, "locomotion", ENGINE_ANIMATION_LAYER_NAME_MAX_LENGTH - 1);
        locomotion.mode = ENGINE_ANIMATION_LAYER_MODE_OVERRIDE;
        locomotion.weight = 1.0f;
        locomotion.looping = true;

        auto& combat = ac.layers[COMBAT_LAYER_ID];
        combat = {};
        std::strncpy(combat.name, "combat", ENGINE_ANIMATION_LAYER_NAME_MAX_LENGTH - 1);
        combat.mode = ENGINE_ANIMATION_LAYER_MODE_OVERRIDE;
        combat.weight = 9.0f;
        constexpr std::array<std::pair<const char*, float>, 4> combat_mask = { {
            { "hips", 0.15f },
            { "upperleg.l", 0.0f },
            { "upperleg.r", 0.0f },
            { "spine", 1.0f },
        } };
        combat.joint_weights_count = static_cast<std::uint32_t>(combat_mask.size());
        for (std::size_t i = 0; i < combat_mask.size(); i++)
        {
            std::strncpy(combat.joint_weights[i].joint_name, combat_mask[i].first, ENGINE_ANIMATION_JOINT_NAME_MAX_LENGTH - 1);
            combat.joint_weights[i].weight = combat_mask[i].second;
        }
        engineGameObjectUpdateAnimationControllerComponent(go_, &ac);
    }
}

project_c::Player::~Player()
{
    for (const auto& [animation, id] : animation_events_)
    {
        engineAnimationControllerAnimationRemoveEvent(go_, animation, id);
    }
    // its runtime parts and the weapon it spawned; their scripts go with their game objects. A weapon of the level it
    // holds stays.
    const bool spawned_weapon = engineGameObjectIsValid(weapon_go_) && engineGameObjectIsTransient(weapon_go_);
    for (const auto runtime_go : { attack_trigger_go_, spawned_weapon ? weapon_go_ : ENGINE_INVALID_GAME_OBJECT_ID })
    {
        if (engineGameObjectIsValid(runtime_go))
        {
            engineGameObjectDestroy(runtime_go);
        }
    }
}

project_c::AttackTrigger* project_c::Player::attack_trigger() const
{
    return my_scene_->get_script<AttackTrigger>(attack_trigger_go_);
}

project_c::Weapon* project_c::Player::weapon() const
{
    return my_scene_->get_script<Weapon>(weapon_go_);
}

void project_c::Player::set_attack_window_active(bool active)
{
    if (auto* trigger = attack_trigger())
    {
        active ? trigger->activate() : trigger->deactivate();
    }
}

project_c::PlayerTuning project_c::Player::tuning() const
{
    const auto& types = game_components();
    return get_data_asset(get_game_component(go_, types.player_config, default_player_config()).tuning, types.player_tuning, default_player_tuning());
}

project_c::TraversalTuning project_c::Player::traversal_tuning() const
{
    const auto& types = game_components();
    return get_data_asset(get_game_component(go_, types.player_config, default_player_config()).traversal, types.traversal_tuning, default_traversal_tuning());
}

void project_c::Player::on_start()
{
    // the level starts with what the player entered it with; a fresh player at the start of
    // the game
    const auto& app = *static_cast<const AppProjectC*>(my_scene_->get_app());
    auto& stats = edit_state();
    stats = app.get_level_entry_state().value_or(fresh_state());
    stats.hp = std::min(stats.hp, tuning().max_hp);
    const auto carried_weapon = std::exchange(stats.weapon, WeaponType::eNone);
    // runtime parts of the player, never saved: the attack trigger, the traversal marker and the weapon
    attack_trigger_go_ = my_scene_->register_script<AttackTrigger>(engineGameObjectCreateTransient(), go_)->get_game_object();
    traversal_.create_marker();
    // held again when it is spawned and equipped (equip_waepon)
    if (carried_weapon == WeaponType::eDaggers)
    {
        const auto weapon_go = engineGameObjectCreateTransient();
        Weapon::add_components(weapon_go, app);
        equip_waepon(my_scene_->register_script<Weapon>(weapon_go));
    }

    for (std::size_t combo_index = 0; combo_index < AttackStateData::combo_length; ++combo_index)
    {
        const auto& attack = attack_data_.get_attack(combo_index);
        float duration = 0.0f;
        engineAnimationControllerAnimationGetDuration(go_, attack.animation_name, &duration);
        assert(duration > 0.0f);

        for (std::size_t window_index = 0; window_index < attack.hit_windows_count; ++window_index)
        {
            const auto& window = attack.hit_windows[window_index];
            assert(window.start < window.end && window.end < duration);

            engine_animation_event_t open_event{};
            open_event.trigger_time = window.start;
            open_event.user_data = this;
            open_event.fn_ptr = attack_window_open_callback;
            engine_animation_event_id_t open_event_id = ENGINE_INVALID_OBJECT_HANDLE;
            const auto open_result = engineAnimationControllerAnimationAddEvent(go_, attack.animation_name, open_event, &open_event_id);
            assert(open_result == ENGINE_RESULT_CODE_OK && open_event_id != ENGINE_INVALID_OBJECT_HANDLE);
            animation_events_.emplace_back(attack.animation_name, open_event_id);

            engine_animation_event_t close_event{};
            close_event.trigger_time = window.end;
            close_event.user_data = this;
            close_event.fn_ptr = attack_window_close_callback;
            engine_animation_event_id_t close_event_id = ENGINE_INVALID_OBJECT_HANDLE;
            const auto close_result = engineAnimationControllerAnimationAddEvent(go_, attack.animation_name, close_event, &close_event_id);
            assert(close_result == ENGINE_RESULT_CODE_OK && close_event_id != ENGINE_INVALID_OBJECT_HANDLE);
            animation_events_.emplace_back(attack.animation_name, close_event_id);
        }
    }
}

void project_c::Player::update(float dt)
{
    engine::ScopedProfiler prof("project_c::Player::update");
    const auto t = tuning();
    const auto traversal = traversal_tuning();
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
        // still falls, e.g. when it died on a wall
        if (!engineGameObjectHasCharacterControllerComponent(go_) || engineCharacterControllerMove(go_, glm::value_ptr(glm::vec3(0.0f)), dt, nullptr) != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed to move the dead player.\n");
        }
        return;
    }
    dodge_cooldown_remaining_seconds_ = std::max(0.0f, dodge_cooldown_remaining_seconds_ - dt / 1000.0f);

    // presses are detected every frame, also during hit-stop, and kept until the player can act on them;
    // otherwise a press during a hit-stop (e.g. the next combo attack right after a confirmed hit) is lost
    attack_press_pending_ = attack_press_pending_ || input().attack;
    dodge_press_pending_ = dodge_press_pending_ || input().dodge;
    drop_weapon_press_pending_ = drop_weapon_press_pending_ || input().drop_weapon;

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
        if (!engineAnimationControllerSetPaused(go_, false))
        {
            engineLog("Failed to resume player animation after hit-stop.\n");
        }
    }

    // [DEBUG] reset position  ToDo: remove it later
    if (input().debug_reset_position)
    {
        set_world_position( 0.0f, 1.0f, 0.0f );
    }

    // what is under the cursor (triggers, like the attack trigger, are skipped)
    const auto active_camera_go = utils::get_active_camera_game_objects()[0];
    const auto ray = utils::get_ray_from_cursor(active_camera_go, input().cursor);
    engine_ray_hit_t hit_info{};
    const bool cursor_hit = enginePhysicsRayCast(glm::value_ptr(ray.origin), glm::value_ptr(ray.direction), glm::length(ray.direction), nullptr, &hit_info);

    const auto lmb = input().interact;
    // like during hit-stop, an attack or dodge pressed in the air, during a scripted move or while landing waits until
    // the player can act (on a wall the dodge press lets go of it)
    const bool presses_wait = movement_mode_ == MovementMode::eAirborne
        || movement_mode_ == MovementMode::eScriptedMove
        || (movement_mode_ == MovementMode::eGrounded && landing_recovery_remaining_seconds_ > 0.0f);
    const bool attack_pressed = !presses_wait && std::exchange(attack_press_pending_, false);
    const bool dodge_pressed = !presses_wait && std::exchange(dodge_press_pending_, false);
    const bool drop_weapon_pressed = std::exchange(drop_weapon_press_pending_, false);

    const auto button_A = input().left;
    const auto button_W = input().forward;
    const auto button_D = input().right;
    const auto button_S = input().back;
    glm::vec2 move_input(0.0f);
    if (button_A) move_input.x -= 1.0f;
    if (button_D) move_input.x += 1.0f;
    if (button_W) move_input.y -= 1.0f;
    if (button_S) move_input.y += 1.0f;
    if (glm::length(move_input) > 1.0f)
    {
        move_input = glm::normalize(move_input);
    }

    glm::vec3 move_direction(0.0f);
    if (glm::length(move_input) > 0.0f)
    {
        move_direction = glm::normalize(glm::vec3(move_input.x, 0.0f, move_input.y));
    }

    traversal_.set_held_weapon(weapon_go_);
    switch (movement_mode_)
    {
    case MovementMode::eAirborne:
        set_movement_mode(traversal_.update_airborne(move_direction, t.move_speed, t.movement_acceleration, traversal, dt / 1000.0f), traversal);
        return;
    case MovementMode::eClimbing:
        // space lets go of the wall
        set_movement_mode(traversal_.update_climbing(glm::vec3(move_input.x, 0.0f, move_input.y), dodge_pressed, traversal, dt / 1000.0f), traversal);
        return;
    case MovementMode::eScriptedMove:
        set_movement_mode(traversal_.update_scripted_move(dt / 1000.0f), traversal);
        return;
    case MovementMode::eGrounded:
        break;
    }

    const float delta_seconds = dt / 1000.0f;
    // after a landing the player can't act for a moment
    const bool recovering = landing_recovery_remaining_seconds_ > 0.0f;
    if (recovering)
    {
        landing_recovery_remaining_seconds_ = std::max(0.0f, landing_recovery_remaining_seconds_ - delta_seconds);
        if (landing_recovery_remaining_seconds_ <= 0.0f)
        {
            engineAnimationControllerLayerSetLooping(go_, LOCOMOTION_LAYER_ID, true);
            engineAnimationControllerLayerSetWeight(go_, COMBAT_LAYER_ID, 1.0f);
        }
    }

    if (weapon() && attack_pressed && !dodge_pressed && !recovering && dodge_remaining_seconds_ <= 0.0f && hit_stun_remaining_seconds_ <= 0.0f)
    {
        if (!check_state_bit(States::ATTACK))
        {
            start_attack(0);
        }
        else if (attack_combo_index_ + 1 < AttackStateData::combo_length)
        {
            attack_buffered_ = true;
            attack_buffer_remaining_ = t.attack_input_buffer_seconds;
        }
    }

    if (cursor_hit && lmb)
    {
        if (auto* interactable = my_scene_->get_script<Interactable>(hit_info.go))
        {
            // check distance and interface if close enough
            const auto tc = engineGameObjectGetTransformComponent(go_);
            const float distance = glm::distance(glm::make_vec3(hit_info.position), glm::vec3(tc.position[0], tc.position[1], tc.position[2]));
            if (distance < 1.0f)
            {
                interactable->interact();
            }
        }
    }

    if (auto* held = weapon(); held && drop_weapon_pressed)
    {
        // drop weapon
        const auto tc = engineGameObjectGetTransformComponent(go_);
        held->drop_on_ground({ tc.position[0], tc.position[1], tc.position[2] });
        weapon_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
        edit_state().weapon = WeaponType::eNone;
    }

    const auto tc = engineGameObjectGetTransformComponent(go_);
    const glm::quat rotation = glm::make_quat(tc.rotation);
    const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);

    // walking into a wall / window / roof edge starts a traversal, a dodge at an edge leaps off it (not in the middle of
    // an attack, a dodge, a stun or a landing)
    const bool can_dodge = dodge_pressed
        && dodge_cooldown_remaining_seconds_ <= 0.0f
        && hit_stun_remaining_seconds_ <= 0.0f
        && !check_state_bit(States::ATTACK)
        && !recovering;
    PlayerTraversal::GroundInput traversal_input{};
    traversal_input.position = glm::vec3(tc.position[0], tc.position[1], tc.position[2]);
    traversal_input.move_direction = move_direction;
    traversal_input.approach_speed = glm::dot(ground_velocity_, move_direction);
    traversal_input.can_start = !check_state_bit(States::ATTACK) && dodge_remaining_seconds_ <= 0.0f && hit_stun_remaining_seconds_ <= 0.0f && !recovering;
    traversal_input.leap_pressed = can_dodge && traversal_input.can_start;
    if (const auto mode = traversal_.update_grounded(traversal_input, traversal, delta_seconds); mode != MovementMode::eGrounded)
    {
        if (mode == MovementMode::eAirborne)
        {
            // the leap was the dodge
            dodge_cooldown_remaining_seconds_ = t.dodge_cooldown_seconds;
        }
        set_movement_mode(mode, traversal);
        return;
    }

    if (can_dodge)
    {
        start_dodge(move_input, rotation);
    }

    bool dodging_this_frame = dodge_remaining_seconds_ > 0.0f;
    float movement_speed = 0.0f;
    // horizontal velocity of this frame; the character controller adds falling and collisions
    glm::vec3 velocity(0.0f);
    if (dodging_this_frame)
    {
        movement_velocity_ = glm::vec2(0.0f);
        velocity = glm::vec3(dodge_direction_.x, 0.0f, dodge_direction_.z) * t.dodge_speed;
        dodge_remaining_seconds_ = std::max(0.0f, dodge_remaining_seconds_ - delta_seconds);
        dodge_invulnerability_remaining_seconds_ = std::max(0.0f, dodge_invulnerability_remaining_seconds_ - delta_seconds);
        if (dodge_remaining_seconds_ <= 0.0f)
        {
            const bool locomotion_loops = engineAnimationControllerLayerSetLooping(go_, LOCOMOTION_LAYER_ID, true);
            const bool combat_layer_restored = engineAnimationControllerLayerSetWeight(go_, COMBAT_LAYER_ID, 1.0f);
            assert(locomotion_loops && combat_layer_restored);
        }
    }
    else if (hit_stun_remaining_seconds_ > 0.0f)
    {
        movement_velocity_ = glm::vec2(0.0f);
        velocity = glm::vec3(knockback_velocity_.x, 0.0f, knockback_velocity_.z);
        knockback_velocity_ *= std::exp(-10.0f * delta_seconds);
        hit_stun_remaining_seconds_ = std::max(0.0f, hit_stun_remaining_seconds_ - delta_seconds);
    }
    else
    {
        // landing: brakes, whatever the input
        const glm::vec2 wanted = recovering ? glm::vec2(0.0f) : move_input;
        const float target_speed = t.move_speed;
        const glm::vec2 target_velocity = wanted * target_speed;
        const glm::vec2 velocity_delta = target_velocity - movement_velocity_;
        const float max_velocity_change = (glm::length(wanted) > 0.0f ? t.movement_acceleration : t.movement_braking) * delta_seconds;
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
        if (movement_speed < PLAYER_IDLE_SPEED_THRESHOLD && glm::length(wanted) == 0.0f)
        {
            movement_velocity_ = glm::vec2(0.0f);
            movement_speed = 0.0f;
        }
        velocity = glm::vec3(movement_velocity_.x, 0.0f, movement_velocity_.y);
    }
    engine_character_controller_move_result_t move_result{};
    if (engineCharacterControllerMove(go_, glm::value_ptr(velocity), dt, &move_result) != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed to move the player.\n");
    }
    else if (delta_seconds > 0.0f)
    {
        const auto moved = engineGameObjectGetTransformComponent(go_);
        ground_velocity_ = glm::vec3(moved.position[0] - tc.position[0], 0.0f, moved.position[2] - tc.position[2]) / delta_seconds;
        // walked (or dodged) off the ground: falling; held up by what isn't walkable ground (a too steep slope, an
        // edge) it walks on
        ungrounded_seconds_ = move_result.grounded || move_result.blocked_below ? 0.0f : ungrounded_seconds_ + delta_seconds;
        if (ungrounded_seconds_ >= FALL_DELAY_SECONDS)
        {
            set_movement_mode(traversal_.start_falling(ground_velocity_), traversal);
            return;
        }
    }

    // while recovering, the landing animation plays
    if (!recovering && !dodging_this_frame && movement_speed > PLAYER_IDLE_SPEED_THRESHOLD)
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
        const bool playback_rate_set = engineAnimationControllerLayerSetPlaybackRate(go_, LOCOMOTION_LAYER_ID, playback_rate);
        assert(playback_rate_set);

        if (!engineAnimationControllerIsAnimationPlayingOnLayer(go_, animation_name, LOCOMOTION_LAYER_ID))
        {
            engineAnimationControllerAnimationCrossFade(go_, animation_name, LOCOMOTION_LAYER_ID, 0.15f);
        }
    }
    else if (!recovering)
    {
        const bool playback_rate_set = engineAnimationControllerLayerSetPlaybackRate(go_, LOCOMOTION_LAYER_ID, 1.0f);
        assert(playback_rate_set);
        if (!engineAnimationControllerIsAnimationPlayingOnLayer(go_, "Unarmed_Idle", LOCOMOTION_LAYER_ID))
        {
            engineAnimationControllerAnimationCrossFade(go_, "Unarmed_Idle", LOCOMOTION_LAYER_ID, 0.15f);
        }
    }
    if (check_state_bit(States::ATTACK))
    {
        attack_elapsed_ += t.attack_playback_rate * dt / 1000.0f;
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

        const bool attack_is_playing = engineAnimationControllerIsAnimationPlayingOnLayer(go_, attack.animation_name, COMBAT_LAYER_ID);
        const bool combo_window_open = attack_elapsed_ >= attack.combo_window_start && attack_elapsed_ < attack.combo_window_end;
        if (attack_buffered_ && combo_window_open)
        {
            attack_buffered_ = false;
            start_attack(attack_combo_index_ + 1);
        }
        else if (!attack_is_playing || attack_elapsed_ >= attack.exit_time)
        {
            if (attack_buffered_ && attack_combo_index_ + 1 < AttackStateData::combo_length)
            {
                attack_buffered_ = false;
                start_attack(attack_combo_index_ + 1);
            }
            else
            {
                clear_state_bit(States::ATTACK);
                attack_phase_ = AttackPhase::eIdle;
                set_attack_window_active(false);
                attack_buffered_ = false;
                const bool combat_rate_reset = engineAnimationControllerLayerSetPlaybackRate(go_, COMBAT_LAYER_ID, 1.0f);
                assert(combat_rate_reset);
                if (engineAnimationControllerAnimationCrossFade(go_, "Unarmed_Idle", COMBAT_LAYER_ID, t.attack_exit_cross_fade_seconds))
                {
                    const bool combat_idle_loops = engineAnimationControllerLayerSetLooping(go_, COMBAT_LAYER_ID, true);
                    assert(combat_idle_loops);
                }
                else
                {
                    engineLog("Failed to cross-fade combat layer back to idle.\n");
                }
            }
        }
    }

    if (cursor_hit && attack_phase_ != AttackPhase::eActive && !dodging_this_frame && hit_stun_remaining_seconds_ <= 0.0f)
    {
        auto facing_transform = engineGameObjectGetTransformComponent(go_);
        const glm::vec3 origin(facing_transform.position[0], facing_transform.position[1], facing_transform.position[2]);
        const glm::vec3 target = glm::make_vec3(hit_info.position);
        const glm::vec3 target_direction(target.x - origin.x, 0.0f, target.z - origin.z);
        if (glm::dot(target_direction, target_direction) > 0.000001f)
        {
            const glm::quat current_rotation = glm::normalize(glm::make_quat(facing_transform.rotation));
            const glm::vec3 current_forward = current_rotation * glm::vec3(0.0f, 0.0f, 1.0f);
            const float current_yaw = std::atan2(current_forward.x, current_forward.z);
            const float target_yaw = std::atan2(target_direction.x, target_direction.z);
            const float yaw_delta = std::atan2(std::sin(target_yaw - current_yaw), std::cos(target_yaw - current_yaw));
            const float max_turn = glm::radians(t.turn_rate_degrees_per_second) * (dt / 1000.0f);
            const float turn_step = std::clamp(yaw_delta, -max_turn, max_turn);
            const glm::quat rotation = glm::angleAxis(current_yaw + turn_step, glm::vec3(0.0f, 1.0f, 0.0f));
            std::memcpy(facing_transform.rotation, glm::value_ptr(rotation), sizeof(facing_transform.rotation));
            engineGameObjectUpdateTransformComponent(go_, &facing_transform);
        }
    }
}

const project_c::PlayerInput& project_c::Player::input() const
{
    return static_cast<const AppProjectC*>(my_scene_->get_app())->player_input();
}

bool project_c::Player::equip_waepon(Weapon* sword)
{
    if (!weapon() && sword)
    {
        weapon_go_ = sword->get_game_object();
        edit_state().weapon = sword->get_type();
        sword->attach_to_game_object(go_, "handslot.r", glm::vec3(0.0f, 0.0f, 0.0f), glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        sword->attach_offhand_to_game_object(go_, "handslot.l");
        return true;
    }
    return false;
}

void project_c::Player::on_confirmed_attack_hit(float hit_stop_seconds)
{
    const auto t = tuning();
    if ((state_ & States::DEAD) != 0)
    {
        return;
    }
    hit_stop_remaining_seconds_ = std::max(hit_stop_remaining_seconds_, hit_stop_seconds);
    hit_stop_just_started_ = true;
    if (!engineAnimationControllerSetPaused(go_, true))
    {
        engineLog("Failed to pause player animation for hit-stop.\n");
    }

    const auto cameras = utils::get_active_camera_game_objects();
    if (!cameras.empty())
    {
        my_scene_->get_script<CameraScript>(cameras.front())->shake(
            t.hit_camera_shake_intensity,
            t.hit_camera_shake_seconds);
    }
}

bool project_c::Player::receive_damage(std::uint32_t damage, const glm::vec3& source_position)
{
    const auto t = tuning();
    auto& hp = edit_state().hp;
    if (hp == 0 || dodge_invulnerability_remaining_seconds_ > 0.0f)
    {
        return false;
    }

    hp = damage >= hp ? 0 : hp - damage;

    auto player_transform = engineGameObjectGetTransformComponent(go_);
    glm::vec3 knockback_direction(
        player_transform.position[0] - source_position.x,
        0.0f,
        player_transform.position[2] - source_position.z);
    if (glm::dot(knockback_direction, knockback_direction) <= 0.0001f)
    {
        knockback_direction = glm::make_quat(player_transform.rotation) * glm::vec3(0.0f, 0.0f, 1.0f);
    }
    // off the ground the hit hurts but neither stuns nor pushes (they are only applied on the ground)
    const bool grounded = movement_mode_ == MovementMode::eGrounded;
    knockback_velocity_ = grounded ? glm::normalize(knockback_direction) * t.knockback_speed : glm::vec3(0.0f);

    if (hp == 0)
    {
        // fall off the wall
        traversal_.cancel();
        set_movement_mode(MovementMode::eGrounded, traversal_tuning());
        state_ |= States::DEAD;
        movement_velocity_ = glm::vec2(0.0f);
        knockback_velocity_ = glm::vec3(0.0f);
        set_attack_window_active(false);
        if (!engineAnimationControllerSetPaused(go_, false))
        {
            engineLog("Failed to resume player animation before death.\n");
        }
        const bool combat_layer_hidden = engineAnimationControllerLayerSetWeight(go_, COMBAT_LAYER_ID, 0.0f);
        const bool locomotion_stopped_looping = engineAnimationControllerLayerSetLooping(go_, LOCOMOTION_LAYER_ID, false);
        assert(combat_layer_hidden && locomotion_stopped_looping);
        if (!engineAnimationControllerAnimationPlay(go_, "Death_A", LOCOMOTION_LAYER_ID))
        {
            engineLog("Failed to play player death animation.\n");
        }
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
                set_attack_window_active(false);
                engineAnimationControllerLayerSetPlaybackRate(go_, COMBAT_LAYER_ID, 1.0f);
                if (!engineAnimationControllerAnimationCrossFade(go_, "Unarmed_Idle", COMBAT_LAYER_ID, 0.08f))
                {
                    engineLog("Failed to interrupt player attack after taking damage.\n");
                }
            }
        }

        hit_stun_remaining_seconds_ = grounded ? t.hit_stun_seconds : 0.0f;
        hit_stop_remaining_seconds_ = std::max(hit_stop_remaining_seconds_, 0.04f);
        hit_stop_just_started_ = true;
        if (!engineAnimationControllerSetPaused(go_, true))
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

const project_c::PlayerState& project_c::Player::state() const
{
    return static_cast<const AppProjectC*>(my_scene_->get_app())->player_state();
}

project_c::PlayerState& project_c::Player::edit_state()
{
    return static_cast<AppProjectC*>(my_scene_->get_app())->player_state();
}

project_c::PlayerState project_c::Player::fresh_state() const
{
    return PlayerState{ tuning().max_hp, 0, WeaponType::eDaggers };
}

void project_c::Player::add_coin(std::uint64_t amount)
{   
    edit_state().coins += amount;
    // for now only log, but we should update UI
    engineLog(std::format("Player coins: {}\n", state().coins).c_str());
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
        return PLAYER_WALK_ANIMATION_SPEED;
    }
    if (dir == Direction::eLeft || dir == Direction::eRight)
    {
        return PLAYER_RUN_ANIMATION_SPEED;
    }
    return PLAYER_RUN_ANIMATION_SPEED;
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

bool project_c::Player::start_attack(std::size_t combo_index)
{
    const auto t = tuning();
    if (combo_index >= AttackStateData::combo_length)
    {
        return false;
    }

    const auto animation_name = attack_data_.get_animation_name(combo_index);
    float duration = 0.0f;
    engineAnimationControllerAnimationGetDuration(go_, animation_name, &duration);
    // Checked before the combat layer is touched: on failure the layer stays as it was (idle, or the attack which is still playing).
    // Play and CrossFade fail without changing the layer.
    const bool animation_started = duration > 0.0f && (combo_index == 0 && !combat_layer_has_pose_
        ? engineAnimationControllerAnimationPlay(go_, animation_name, COMBAT_LAYER_ID)
        : engineAnimationControllerAnimationCrossFade(go_,
            animation_name,
            COMBAT_LAYER_ID,
            combo_index == 0 ? t.attack_exit_cross_fade_seconds : t.attack_combo_cross_fade_seconds));
    if (!animation_started)
    {
        engineLog(std::format("Failed to start attack animation '{}'.\n", animation_name).c_str());
        return false;
    }

    const bool combat_layer_stopped_looping = engineAnimationControllerLayerSetLooping(go_, COMBAT_LAYER_ID, false);
    const bool combat_rate_set = engineAnimationControllerLayerSetPlaybackRate(go_, COMBAT_LAYER_ID, t.attack_playback_rate);
    assert(combat_layer_stopped_looping && combat_rate_set);

    if (auto* trigger = attack_trigger())
    {
        trigger->begin_swing();
    }
    attack_combo_index_ = combo_index;
    attack_elapsed_ = 0.0f;
    attack_phase_ = AttackPhase::eWindup;
    attack_buffered_ = false;
    state_ |= States::ATTACK;
    combat_layer_has_pose_ = true;
    return true;
}

bool project_c::Player::start_dodge(const glm::vec2& move_input, const glm::quat& rotation)
{
    const auto t = tuning();
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
    engineAnimationControllerAnimationGetDuration(go_, animation_name, &duration);
    if (duration <= 0.0f)
    {
        engineLog(std::format("Dodge animation '{}' has no valid duration.\n", animation_name).c_str());
        return false;
    }

    const bool locomotion_stopped_looping = engineAnimationControllerLayerSetLooping(go_, LOCOMOTION_LAYER_ID, false);
    const bool locomotion_rate_reset = engineAnimationControllerLayerSetPlaybackRate(go_, LOCOMOTION_LAYER_ID, 1.0f);
    const bool combat_layer_hidden = engineAnimationControllerLayerSetWeight(go_, COMBAT_LAYER_ID, 0.0f);
    if (!locomotion_stopped_looping || !locomotion_rate_reset || !combat_layer_hidden
        || !engineAnimationControllerAnimationCrossFade(go_, animation_name, LOCOMOTION_LAYER_ID, 0.05f))
    {
        engineLog("Failed to start player dodge.\n");
        engineAnimationControllerLayerSetLooping(go_, LOCOMOTION_LAYER_ID, true);
        engineAnimationControllerLayerSetWeight(go_, COMBAT_LAYER_ID, 1.0f);
        return false;
    }

    dodge_direction_ = world_direction;
    dodge_remaining_seconds_ = duration;
    dodge_invulnerability_remaining_seconds_ = std::min(t.dodge_invulnerability_seconds, duration);
    dodge_cooldown_remaining_seconds_ = t.dodge_cooldown_seconds;
    movement_velocity_ = glm::vec2(0.0f);
    return true;
}

void project_c::Player::set_movement_mode(MovementMode mode, const TraversalTuning& traversal)
{
    const auto previous = std::exchange(movement_mode_, mode);
    if (previous == mode)
    {
        return;
    }
    if (previous == MovementMode::eGrounded)
    {
        // combat (the upper body layer) and walking stop off the ground
        cancel_combat_actions();
        engineAnimationControllerLayerSetWeight(go_, COMBAT_LAYER_ID, 0.0f);
        movement_velocity_ = glm::vec2(0.0f);
        ground_velocity_ = glm::vec3(0.0f);
        ungrounded_seconds_ = 0.0f;
        landing_recovery_remaining_seconds_ = 0.0f;
    }
    if (mode == MovementMode::eGrounded)
    {
        // nothing from before is carried onto the ground: the next grounded move measures the speed again (a traversal
        // started on the first frame must not see the speed from before the fall or the climb), and a hit taken off
        // the ground neither stuns nor pushes
        ground_velocity_ = glm::vec3(0.0f);
        hit_stun_remaining_seconds_ = 0.0f;
        knockback_velocity_ = glm::vec3(0.0f);
        engineAnimationControllerLayerSetPlaybackRate(go_, LOCOMOTION_LAYER_ID, 1.0f);
        // a landing, unless the player died in the air (its death plays instead)
        landing_recovery_remaining_seconds_ = previous == MovementMode::eAirborne && !is_dead() ? traversal.landing_recovery_seconds : 0.0f;
        if (landing_recovery_remaining_seconds_ > 0.0f)
        {
            // the landing plays out before the player acts again (the grounded update ends it)
            engineAnimationControllerLayerSetLooping(go_, LOCOMOTION_LAYER_ID, false);
            engineAnimationControllerAnimationCrossFade(go_, LANDING_ANIMATION, LOCOMOTION_LAYER_ID, 0.05f);
        }
        else
        {
            // locomotion code picks idle/run animation in the next update
            engineAnimationControllerLayerSetLooping(go_, LOCOMOTION_LAYER_ID, true);
            engineAnimationControllerLayerSetWeight(go_, COMBAT_LAYER_ID, 1.0f);
        }
    }
}

void project_c::Player::cancel_combat_actions()
{
    if ((state_ & States::ATTACK) != 0)
    {
        state_ &= ~States::ATTACK;
        attack_phase_ = AttackPhase::eIdle;
        attack_buffered_ = false;
        set_attack_window_active(false);
        engineAnimationControllerLayerSetPlaybackRate(go_, COMBAT_LAYER_ID, 1.0f);
        if (engineAnimationControllerAnimationCrossFade(go_, "Unarmed_Idle", COMBAT_LAYER_ID, 0.1f))
        {
            engineAnimationControllerLayerSetLooping(go_, COMBAT_LAYER_ID, true);
        }
    }
    dodge_remaining_seconds_ = 0.0f;
    dodge_invulnerability_remaining_seconds_ = 0.0f;
    hit_stun_remaining_seconds_ = 0.0f;
    knockback_velocity_ = glm::vec3(0.0f);
}
