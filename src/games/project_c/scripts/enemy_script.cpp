#include "enemy_script.h"
#include "player_script.h"
#include "enviorment_script.h"
#include "scripts_utils.h"
#include "coin_script.h"

#include "../app.h"
#include "encounter_script.h"

#include "../nav_mesh.h"

#include "iscene.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
struct EnemyAttackAnimation
{
    const char* name;
    float active_start;  // normalized animation time when the blade hitbox starts dealing damage
    float active_end;    // normalized animation time when the blade hitbox stops dealing damage
};

// Damage windows must be tuned per animation, because the damage comes from the blade hitbox itself
// (unlike the player's fixed box in front of the character). The windows were measured by sampling
// the "handslot.r" joint of Skeleton_Warrior.glb and checking when the blade sweeps the area in front of the skeleton:
//  - Slice_Horizontal: wind-up to the left ends at ~18%, strike crosses the front at ~20-30%,
//    and from 30% on the blade is in follow-through at ~80 degrees to the right (a shared 30-62% window always missed).
//  - Slice_Diagonal: wind-up held until ~32%, strike crosses the front at ~34-46%.
// The windows are slightly padded: they were measured while the blade collider still lagged one frame behind the
// animated pose (Scene::update now computes world transforms before physics, so it no longer does).
constexpr std::array<EnemyAttackAnimation, 2> ENEMY_ATTACK_ANIMATIONS = { {
    { "1H_Melee_Attack_Slice_Diagonal",   0.32f, 0.50f },
    { "1H_Melee_Attack_Slice_Horizontal", 0.18f, 0.36f },
} };
// the blade only reaches ~0.3 m in front of the skeleton, so the player is hit up to ~0.5 m between centers;
// swinging from further away always misses
constexpr float ENEMY_ATTACK_RANGE = 0.5f;
// closer than this there is no nav mesh path to follow: the enemy walks straight at the player
constexpr float ENEMY_DIRECT_APPROACH_RANGE = 0.8f;
// player climbing a wall or standing on a roof is out of reach
constexpr float ENEMY_MAX_REACH_HEIGHT = 0.6f;
constexpr float ENEMY_ATTACK_COOLDOWN_SECONDS = 0.8f;
constexpr std::uint32_t ENEMY_ATTACK_DAMAGE = 2;

void enemy_attack_window_open_callback(const engine_animation_event_info_t* info, void* user_data)
{
    assert(info && user_data);
    static_cast<project_c::Enemy*>(user_data)->set_attack_window_active(true);
}

void enemy_attack_window_close_callback(const engine_animation_event_info_t* info, void* user_data)
{
    assert(info && user_data);
    static_cast<project_c::Enemy*>(user_data)->set_attack_window_active(false);
}
} // namespace

project_c::EnemyWeapon::EnemyWeapon(engine::IScene* my_scene, engine_game_object_t go, Enemy* enemy)
    : BaseNode(my_scene, go, "enemy-weapon")
    , enemy_(enemy)
{
    assert(enemy_);

    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;
    tc.scale[0] = 1.0f;
    tc.scale[1] = 1.0f;
    tc.scale[2] = 1.0f;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    auto pc = engineGameObjectAddParentComponent(go_);
    pc.parent = enemy_->get_game_object();
    engineGameObjectUpdateParentComponent(go_, &pc);

    auto jac = engineGameObjectAddJointAttachmentComponent(go_);
    jac.skin = pc.parent;
    engineStringSet(jac.joint_name, "handslot.r");
    engineGameObjectUpdateJointAttachmentComponent(go_, &jac);

    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    cc.is_trigger = true;
    auto& hitbox = cc.collider.compound.children[0];
    hitbox.type = ENGINE_COLLIDER_TYPE_BOX;
    hitbox.transform[0] = 0.0f;
    hitbox.transform[1] = 0.15f;
    hitbox.transform[2] = 0.0f;
    hitbox.rotation_quaternion[3] = 1.0f;
    set_c_array(hitbox.collider.box.size, std::array<float, 3>{ 0.10f, 0.26f, 0.045f });
    engineGameObjectUpdateColliderComponent(go_, &cc);
}

void project_c::EnemyWeapon::begin_swing()
{
    attack_window_active_ = false;
    hit_player_this_swing_ = false;
}

void project_c::EnemyWeapon::set_attack_window_active(bool active)
{
    attack_window_active_ = active;
}

void project_c::EnemyWeapon::on_collision(const collision_t& info)
{
    if (!attack_window_active_ || hit_player_this_swing_)
    {
        return;
    }

    if (!engineGameObjectHasNameComponent(info.other)
        || std::strcmp(engineGameObjectGetNameComponent(info.other).name, "player") != 0)
    {
        return;
    }

    hit_player_this_swing_ = true;
    enemy_->on_weapon_hit_player(info.other);
}

project_c::Enemy::Enemy(engine::IScene* my_scene, const PrefabResult& pr, const NavMesh* nav_mesh, float offset_x, float offset_z)
    : BaseNode(my_scene, pr, "enemy")
    , state_(States::DECISION_MAKE)
    , nav_mesh_(nav_mesh)
{
    auto tc = engineGameObjectGetTransformComponent(go_);

    tc.position[0] = offset_x;
    tc.position[1] += 0.25f;
    tc.position[2] = offset_z;

    tc.scale[0] = 0.35f;
    tc.scale[1] = 0.35f;
    tc.scale[2] = 0.35f;

    engineGameObjectUpdateTransformComponent(go_, &tc);
    collect_hit_flash_materials(go_);

    // physics
    auto cc = engineGameObjectAddColliderComponent(go_);
    cc.type = ENGINE_COLLIDER_TYPE_COMPOUND;
    auto& child_c = cc.collider.compound.children[0];
    {
        child_c.type = ENGINE_COLLIDER_TYPE_BOX;
        child_c.transform[0] = 0.0f;
        child_c.transform[1] = 1.0f;
        child_c.transform[2] = 0.0f;
        child_c.rotation_quaternion[3] = 1.0f;
        set_c_array(child_c.collider.box.size, std::array<float, 3>{ 0.7f, 1.0f, 0.5f});
    }
    engineGameObjectUpdateColliderComponent(go_, &cc);

    //rb
    auto rbc = engineGameObjectAddRigidBodyComponent(go_);
    rbc.mass = 100000.0f;
    engineGameObjectUpdateRigidBodyComponent(go_, &rbc);

    auto* app = reinterpret_cast<AppProjectC*>(my_scene_->get_app());
    if (app->is_prefab_available(PREFAB_TYPE_SKELETON_BLADE))
    {
        const auto blade = app->instantiate_prefab(PREFAB_TYPE_SKELETON_BLADE, my_scene_);
        if (blade.go != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            weapon_ = my_scene_->register_script<EnemyWeapon>(blade.go, this);
        }
    }
    else
    {
        engineLog("Skeleton blade prefab is unavailable; enemy weapon visuals and hitbox are disabled.\n");
    }

    const auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;
    for (const auto& attack_animation : ENEMY_ATTACK_ANIMATIONS)
    {
        const char* animation_name = attack_animation.name;
        float duration = 0.0f;
        engineAnimationControllerAnimationGetDuration(animation_controller, animation_name, &duration);
        if (duration <= 0.0f)
        {
            engineLog("Enemy attack animation has no valid duration.\n");
            continue;
        }

        engine_animation_event_t open_event{};
        open_event.trigger_time = attack_animation.active_start * duration;
        open_event.user_data = this;
        open_event.fn_ptr = enemy_attack_window_open_callback;
        engine_animation_event_id_t event_id = ENGINE_INVALID_OBJECT_HANDLE;
        if (engineAnimationControllerAnimationAddEvent(animation_controller, animation_name, open_event, &event_id) != ENGINE_RESULT_CODE_OK
            || event_id == ENGINE_INVALID_OBJECT_HANDLE)
        {
            engineLog("Failed to add enemy attack opening event.\n");
        }

        engine_animation_event_t close_event{};
        close_event.trigger_time = attack_animation.active_end * duration;
        close_event.user_data = this;
        close_event.fn_ptr = enemy_attack_window_close_callback;
        event_id = ENGINE_INVALID_OBJECT_HANDLE;
        if (engineAnimationControllerAnimationAddEvent(animation_controller, animation_name, close_event, &event_id) != ENGINE_RESULT_CODE_OK
            || event_id == ENGINE_INVALID_OBJECT_HANDLE)
        {
            engineLog("Failed to add enemy attack closing event.\n");
        }
    }
}

project_c::Enemy::~Enemy()
{
    //assert(engineGameObjectIsValid(go_));
    utils::delete_game_objects_hierarchy(go_);
}

void project_c::Enemy::update(float dt)
{
    engine::ScopedProfiler prof("project_c::Enemy::update");

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
            engineLog("Failed to resume enemy animation after hit-stop.\n");
        }
    }

    if (hit_flash_remaining_seconds_ > 0.0f)
    {
        hit_flash_remaining_seconds_ = std::max(0.0f, hit_flash_remaining_seconds_ - dt / 1000.0f);
        update_hit_flash(hit_flash_remaining_seconds_ / 0.12f);
    }

    attack_cooldown_remaining_seconds_ = std::max(0.0f, attack_cooldown_remaining_seconds_ - dt / 1000.0f);

    if (player_go_ == ENGINE_INVALID_GAME_OBJECT_ID)
    {
        const auto player_game_objects = utils::get_game_objects_with_name("player");
        if (player_game_objects.empty())
        {
            return;
        }
        player_go_ = player_game_objects.front();
    }

    for (auto& s : debug_scripts_)
    {
        my_scene_->unregister_script(s);
    }
    debug_scripts_.clear();

    auto tc = engineGameObjectGetTransformComponent(go_);
    auto ec = engineGameObjectGetTransformComponent(player_go_);
    const bool player_out_of_reach = std::abs(ec.position[1] - tc.position[1]) > ENEMY_MAX_REACH_HEIGHT;
    const auto distance_to_player = player_out_of_reach
        ? std::numeric_limits<float>::max()
        : glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(ec.position[0], ec.position[2]));

    const auto my_node_idx =  nav_mesh_->get_node_idx({ tc.position[0], tc.position[1], tc.position[2] });
    const auto player_node_idx =  nav_mesh_->get_node_idx({ ec.position[0], ec.position[1], ec.position[2] });
    auto path = [&]()
        {
            if (distance_to_player < ENEMY_DIRECT_APPROACH_RANGE || my_node_idx == -1 || player_node_idx == -1)
            {
                return NavMeshPathFinder::PathFromStartToEnd{};
            }
            return NavMeshPathFinder::find_path(*nav_mesh_, my_node_idx, player_node_idx);
        }();

    if (hp <= 0 && state_ != States::DIE)
    {
        state_ = States::DIE;
        set_attack_window_active(false);
        if (!engineAnimationControllerLayerSetLooping(animation_controller, 0, false))
        {
            engineLog("Failed to disable looping for enemy death.\n");
        }
        if (!engineAnimationControllerAnimationPlay(animation_controller, "Death_A", 0))
        {
            engineLog("Failed to play enemy death animation.\n");
        }
        // remove collider so enemy will not be hit by players attacks
        engineGameObjectRemoveColliderComponent(go_);
        if (reinterpret_cast<AppProjectC*>(my_scene_->get_app())->is_prefab_available(project_c::PrefabType::PREFAB_TYPE_COIN_GOLD))
        {
            auto coin = my_scene_->register_script<project_c::Coin>(reinterpret_cast<AppProjectC*>(my_scene_->get_app())->instantiate_prefab(project_c::PREFAB_TYPE_COIN_GOLD, my_scene_).go);
            coin->set_world_position(tc.position[0], tc.position[1] + 1.0f, tc.position[2]);
        }
    }

    const auto* encounter = my_scene_->find_script<Encounter>();
    if (encounter && encounter->is_over() && state_ != States::DIE)
    {
        return;
    }

    switch (state_)
    {
    case States::DECISION_MAKE:
    {
        if (distance_to_player <= ENEMY_ATTACK_RANGE && attack_cooldown_remaining_seconds_ <= 0.0f)
        {
            state_ = States::ATTACK;
            if (weapon_)
            {
                weapon_->begin_swing();
            }
            if (!engineAnimationControllerLayerSetLooping(animation_controller, 0, false))
            {
                engineLog("Failed to disable looping for enemy attack.\n");
            }
            if (!engineAnimationControllerIsAnimationPlaying(animation_controller, attack_data_.get_animation_name()))
            {
                if (!engineAnimationControllerAnimationPlay(animation_controller, attack_data_.get_animation_name(), 0))
                {
                    engineLog("Failed to start enemy attack animation.\n");
                    state_ = States::DECISION_MAKE;
                    attack_cooldown_remaining_seconds_ = ENEMY_ATTACK_COOLDOWN_SECONDS;
                }
            }
        }
        else if (distance_to_player <= ENEMY_ATTACK_RANGE)
        {
            state_ = States::IDLE;
        }
        else if (path.nodes.size() >= 1 && path.nodes.size() < 3)
        {
            state_ = States::MOVE;
        }
        else if (distance_to_player < ENEMY_DIRECT_APPROACH_RANGE)
        {
            state_ = States::MOVE;
        }
        else
        {
            state_ = States::IDLE;
        }
        break;
    }
    case States::IDLE:
    {
        if (!engineAnimationControllerLayerSetLooping(animation_controller, 0, true))
        {
            engineLog("Failed to enable looping for enemy idle.\n");
        }
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, "Idle"))
        {
            engineAnimationControllerAnimationPlay(animation_controller, "Idle", 0);
        }
        state_ = States::DECISION_MAKE;
        break;
    }
    case States::ATTACK:
    {
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, attack_data_.get_animation_name()))
        {
            state_ = States::DECISION_MAKE;
            set_attack_window_active(false);
            attack_cooldown_remaining_seconds_ = ENEMY_ATTACK_COOLDOWN_SECONDS;
            attack_data_.attack_with_right = !attack_data_.attack_with_right;
        }

        auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
        quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
        std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
        engineGameObjectUpdateTransformComponent(go_, &tc);

        break;
    }
    case States::DIE:
    {
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, "Death_A"))
        {
            my_scene_->unregister_script(this);
        }
        break;
    }
    case States::MOVE:
    {
        // path is computed per_frame, but decision was made frame before
        // so it can happen that target has moved and new position is not reachable or close to current position
        const bool direct_approach = path.nodes.empty() && distance_to_player < ENEMY_DIRECT_APPROACH_RANGE;
        if (distance_to_player <= ENEMY_ATTACK_RANGE || (path.nodes.empty() && !direct_approach))
        {
            state_ = States::DECISION_MAKE;
            break;
        }
        if (!engineAnimationControllerLayerSetLooping(animation_controller, 0, true))
        {
            engineLog("Failed to enable looping for enemy movement.\n");
        }
        if (!engineAnimationControllerIsAnimationPlaying(animation_controller, "Running_A"))
        {
            engineAnimationControllerAnimationPlay(animation_controller, "Running_A", 0);
        }
        for (auto& node : path.nodes)
        {
            const auto n_pos = nav_mesh_->get_node(node).get_center();
            debug_scripts_.push_back(my_scene_->register_script<project_c::DebugPathNode>(n_pos.x, n_pos.z));
        }
        const glm::vec3 target = direct_approach
            ? glm::vec3(ec.position[0], tc.position[1], ec.position[2])
            : nav_mesh_->get_node(path.nodes[0]).get_center();
        auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), target);
        //auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
        quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
        std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
        const float speed_cooef = 0.002f;
        const float speed = speed_cooef * dt;
        const glm::vec3 forward = glm::normalize(quat * glm::vec3(0.0f, 0.0f, 1.0f));
        tc.position[0] += forward.x * speed;
        //tc.position[1] += forward.y * speed;
        tc.position[2] += forward.z * speed;
        engineGameObjectUpdateTransformComponent(go_, &tc);
        //state_ = States::DECISION_MAKE;
        break;
    }
    default:
    {
        engineLog("Unknown enemy state\n");
        break;
    }
    }
}

void project_c::Enemy::set_attack_window_active(bool active)
{
    if (weapon_)
    {
        weapon_->set_attack_window_active(active);
    }
}

void project_c::Enemy::on_weapon_hit_player(engine_game_object_t player_go)
{
    if (state_ != States::ATTACK || hp == 0)
    {
        return;
    }

    if (auto* player = my_scene_->get_script<project_c::Player>(player_go))
    {
        const auto tc = engineGameObjectGetTransformComponent(go_);
        player->receive_damage(ENEMY_ATTACK_DAMAGE, glm::vec3(tc.position[0], tc.position[1], tc.position[2]));
    }
}

void project_c::Enemy::receive_attack_hit(std::uint32_t damage, float hit_stop_seconds)
{
    set_attack_window_active(false);
    hp = damage >= hp ? 0 : hp - damage;
    hit_flash_remaining_seconds_ = 0.12f;
    update_hit_flash(1.0f);

    hit_stop_remaining_seconds_ = std::max(hit_stop_remaining_seconds_, hit_stop_seconds);
    hit_stop_just_started_ = true;
    const auto animation_controller = engineGameObjectGetAnimationControllerComponent(go_).controller;
    if (!engineAnimationControllerSetPaused(animation_controller, true))
    {
        engineLog("Failed to pause enemy animation for hit-stop.\n");
    }
}

void project_c::Enemy::collect_hit_flash_materials(engine_game_object_t game_object)
{
    if (!engineGameObjectIsValid(game_object))
    {
        return;
    }

    if (engineGameObjectHasMaterialComponent(game_object))
    {
        const auto material = engineGameObjectGetMaterialComponent(game_object);
        if (material.type == ENGINE_MATERIAL_TYPE_PONG)
        {
            HitFlashMaterial entry{};
            entry.game_object = game_object;
            std::copy(
                std::begin(material.data.pong.diffuse_color),
                std::end(material.data.pong.diffuse_color),
                entry.original_diffuse_color.begin());
            hit_flash_materials_.push_back(entry);
        }
    }

    if (engineGameObjectHasChildrenComponent(game_object))
    {
        const auto children = engineGameObjectGetChildrenComponent(game_object);
        for (const auto child : children.child)
        {
            if (child != ENGINE_INVALID_GAME_OBJECT_ID)
            {
                collect_hit_flash_materials(child);
            }
        }
    }
}

void project_c::Enemy::update_hit_flash(float strength)
{
    constexpr std::array<float, 3> hit_color = { 1.0f, 0.32f, 0.22f };
    strength = std::clamp(strength, 0.0f, 1.0f);

    for (const auto& entry : hit_flash_materials_)
    {
        if (!engineGameObjectIsValid(entry.game_object) || !engineGameObjectHasMaterialComponent(entry.game_object))
        {
            continue;
        }

        auto material = engineGameObjectGetMaterialComponent(entry.game_object);
        if (material.type != ENGINE_MATERIAL_TYPE_PONG)
        {
            continue;
        }

        for (std::size_t channel = 0; channel < hit_color.size(); ++channel)
        {
            const float original = entry.original_diffuse_color[channel];
            material.data.pong.diffuse_color[channel] = original + (hit_color[channel] - original) * strength;
        }
        engineGameObjectUpdateMaterialComponent(entry.game_object, &material);
    }
}
