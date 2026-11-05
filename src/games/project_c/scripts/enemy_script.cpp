#include "enemy_script.h"
#include "player_script.h"
#include "enviorment_script.h"
#include "scripts_utils.h"
#include "coin_script.h"
#include "enemy_spawner_script.h"

#include "../app.h"
#include "encounter_script.h"
#include "game_components.h"

#include "iscene.h"
#include <utils.h>

#include <fmt/format.h>

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
// its weapon inherits the enemy's scale, so the weapon's hitbox is in the model's units
constexpr float ENEMY_SCALE = project_c::Enemy::MODEL_SCALE;

bool same_attacks(const project_c::EnemyAttack (&a)[project_c::ENEMY_TYPE_MAX_ATTACKS], const project_c::EnemyAttack (&b)[project_c::ENEMY_TYPE_MAX_ATTACKS])
{
    for (std::uint32_t i = 0; i < project_c::ENEMY_TYPE_MAX_ATTACKS; i++)
    {
        if (std::strcmp(a[i].animation, b[i].animation) != 0 || a[i].active_start != b[i].active_start || a[i].active_end != b[i].active_end)
        {
            return false;
        }
    }
    return true;
}
// the blade only reaches ~0.3 m in front of the skeleton, so the player is hit up to ~0.5 m between centers;
// swinging from further away always misses
// closer than this there is no nav mesh path to follow: the enemy walks straight at the player
constexpr float ENEMY_DIRECT_APPROACH_RANGE = 0.8f;
// a suspicious or searching enemy has reached the spot it walks to this close
constexpr float ENEMY_ARRIVE_DISTANCE = 0.5f;
// a searching enemy looks around (turns) this fast where the player was lost
constexpr float ENEMY_LOOK_AROUND_DEGREES_PER_SECOND = 60.0f;
// a killed enemy alerts its pack for this long after the killing blow (the members read it), then it leaves the pack
constexpr float ENEMY_DEATH_ALARM_SECONDS = 0.2f;
// what it looks at to see the player: the chest, above the player's feet
constexpr float PLAYER_CHEST_HEIGHT = 0.5f;
// corners of the path to the player, including the enemy's own position; only the next one is walked to
constexpr std::size_t ENEMY_MAX_PATH_POINTS = 16;
// how far from the enemy and the player the nav mesh is searched for the ends of the path (the player can stand on the
// edge of the floor, outside the nav mesh, which keeps an agent radius away from it)
constexpr float ENEMY_PATH_SEARCH_HALF_EXTENTS[3] = { 1.0f, 2.0f, 1.0f };
// player climbing a wall or standing on a roof is out of reach
constexpr float ENEMY_MAX_REACH_HEIGHT = 0.6f;

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

project_c::EnemyWeapon::EnemyWeapon(engine::IScene* my_scene, engine_game_object_t go, engine_game_object_t enemy)
    : BaseNode(my_scene, go, "enemy-weapon")
    , enemy_go_(enemy)
{

    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] = 0.0f;
    tc.position[1] = 0.0f;
    tc.position[2] = 0.0f;
    tc.scale[0] = 1.0f;
    tc.scale[1] = 1.0f;
    tc.scale[2] = 1.0f;
    engineGameObjectUpdateTransformComponent(go_, &tc);

    auto pc = engineGameObjectAddParentComponent(go_);
    pc.parent = enemy_go_;
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
    // sized in meters, in the weapon's space which has the enemy's scale
    hitbox.transform[0] = 0.0f;
    hitbox.transform[1] = 0.15f / ENEMY_SCALE;
    hitbox.transform[2] = 0.0f;
    hitbox.rotation_quaternion[3] = 1.0f;
    set_c_array(hitbox.collider.box.size, std::array<float, 3>{ 0.10f / ENEMY_SCALE, 0.26f / ENEMY_SCALE, 0.045f / ENEMY_SCALE });
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
    if (auto* enemy = my_scene_->get_script<Enemy>(enemy_go_))
    {
        enemy->on_weapon_hit_player(info.other);
    }
}

project_c::Enemy::Enemy(engine::IScene* my_scene, engine_game_object_t go)
    : BaseNode(my_scene, go)
    , state_(States::DECISION_MAKE)
{
    collect_hit_flash_materials(go_);
    sync_attack_events(tuning());
}

void project_c::Enemy::sync_attack_events(const EnemyType& type)
{
    if (attack_events_made_ && same_attacks(attack_events_for_, type.attacks))
    {
        return;
    }
    remove_attack_events();
    attack_events_made_ = true;
    std::copy(std::begin(type.attacks), std::end(type.attacks), std::begin(attack_events_for_));
    for (const auto& attack : attack_events_for_)
    {
        const char* animation_name = attack.animation;
        if (animation_name[0] == '\0')
        {
            continue;
        }
        float duration = 0.0f;
        engineAnimationControllerAnimationGetDuration(go_, animation_name, &duration);
        if (duration <= 0.0f)
        {
            log(fmt::format("The enemy's attack animation \"{}\" has no valid duration.\n", animation_name));
            continue;
        }

        engine_animation_event_t open_event{};
        open_event.trigger_time = attack.active_start * duration;
        open_event.user_data = this;
        open_event.fn_ptr = enemy_attack_window_open_callback;
        engine_animation_event_id_t event_id = ENGINE_INVALID_OBJECT_HANDLE;
        if (engineAnimationControllerAnimationAddEvent(go_, animation_name, open_event, &event_id) != ENGINE_RESULT_CODE_OK
            || event_id == ENGINE_INVALID_OBJECT_HANDLE)
        {
            engineLog("Failed to add enemy attack opening event.\n");
        }
        else
        {
            animation_events_.emplace_back(animation_name, event_id);
        }

        engine_animation_event_t close_event{};
        close_event.trigger_time = attack.active_end * duration;
        close_event.user_data = this;
        close_event.fn_ptr = enemy_attack_window_close_callback;
        event_id = ENGINE_INVALID_OBJECT_HANDLE;
        if (engineAnimationControllerAnimationAddEvent(go_, animation_name, close_event, &event_id) != ENGINE_RESULT_CODE_OK
            || event_id == ENGINE_INVALID_OBJECT_HANDLE)
        {
            engineLog("Failed to add enemy attack closing event.\n");
        }
        else
        {
            animation_events_.emplace_back(animation_name, event_id);
        }
    }
}

const project_c::EnemyAttack* project_c::Enemy::next_attack(const EnemyType& type)
{
    for (std::uint32_t i = 0; i < ENEMY_TYPE_MAX_ATTACKS; i++)
    {
        attack_index_ %= ENEMY_TYPE_MAX_ATTACKS;
        if (type.attacks[attack_index_].animation[0] != '\0')
        {
            return &type.attacks[attack_index_];
        }
        attack_index_++;
    }
    return nullptr;
}

void project_c::Enemy::remove_attack_events()
{
    for (const auto& [animation, id] : animation_events_)
    {
        engineAnimationControllerAnimationRemoveEvent(go_, animation.c_str(), id);
    }
    animation_events_.clear();
}

project_c::Enemy::~Enemy()
{
    remove_attack_events();
    // its blade, a runtime object; the blade's script goes with it
    if (engineGameObjectIsValid(weapon_go_))
    {
        engineGameObjectDestroy(weapon_go_);
    }
}

project_c::EnemyWeapon* project_c::Enemy::weapon() const
{
    return my_scene_->get_script<EnemyWeapon>(weapon_go_);
}

void project_c::Enemy::find_player()
{
    const auto* player = my_scene_->find_script<Player>();
    player_go_ = player && engineGameObjectIsValid(player->get_game_object()) ? player->get_game_object() : ENGINE_INVALID_GAME_OBJECT_ID;
}

project_c::EnemyType project_c::Enemy::tuning() const
{
    const auto& types = game_components();
    return get_data_asset(get_game_component(go_, types.enemy_config, default_enemy_config()).type, types.enemy_type, default_enemy_type());
}

void project_c::Enemy::on_start()
{
    hp = tuning().max_hp;
    find_player();

    // the blade is part of the enemy's loadout, a runtime object
    auto* app = reinterpret_cast<AppProjectC*>(my_scene_->get_app());
    if (app->is_prefab_available(PREFAB_TYPE_SKELETON_BLADE))
    {
        const auto blade = app->spawn_prefab(PREFAB_TYPE_SKELETON_BLADE);
        if (blade.go != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            my_scene_->register_script<EnemyWeapon>(blade.go, go_);
            weapon_go_ = blade.go;
        }
    }
    else
    {
        engineLog("Skeleton blade prefab is unavailable; enemy weapon visuals and hitbox are disabled.\n");
    }
}

void project_c::Enemy::update(float dt)
{
    engine::ScopedProfiler prof("project_c::Enemy::update");
    const auto cfg = tuning();
    // the attack timings of the type changed (an edit of its file, or another type)
    sync_attack_events(cfg);

    // what it notices of the player (also when there is none: it forgets), then (after a hit stop) what it does about
    // it: a hit alerts it at once, also during the hit stop. A player destroyed since (e.g. deleted in the editor) is
    // looked for again.
    if (!engineGameObjectIsValid(player_go_))
    {
        find_player();
    }
    // also for the killing blow (it isn't dying yet), so the hit alerts it and its pack
    if (state_ != States::DIE)
    {
        const auto own = engineGameObjectGetTransformComponent(go_);
        const glm::vec3 forward = glm::make_quat(own.rotation) * glm::vec3(0.0f, 0.0f, 1.0f);
        awareness_.update(dt / 1000.0f, perceive(cfg.perception, glm::vec3(own.position[0], own.position[1], own.position[2]), forward), cfg.perception);
    }
    damaged_ = false;
    if (hp == 0)
    {
        dying_seconds_ += dt / 1000.0f;
    }

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
            engineLog("Failed to resume enemy animation after hit-stop.\n");
        }
    }

    if (hit_flash_remaining_seconds_ > 0.0f)
    {
        hit_flash_remaining_seconds_ = std::max(0.0f, hit_flash_remaining_seconds_ - dt / 1000.0f);
        update_hit_flash(hit_flash_remaining_seconds_ / 0.12f);
    }

    attack_cooldown_remaining_seconds_ = std::max(0.0f, attack_cooldown_remaining_seconds_ - dt / 1000.0f);

    // a scene without a player (e.g. a test scene), or the player was destroyed (e.g. deleted in the editor)
    if (!engineGameObjectIsValid(player_go_))
    {
        return;
    }

    for (auto& s : debug_scripts_)
    {
        my_scene_->unregister_script(s);
    }
    debug_scripts_.clear();

    auto tc = engineGameObjectGetTransformComponent(go_);
    auto ec = engineGameObjectGetTransformComponent(player_go_);
    const glm::vec3 position(tc.position[0], tc.position[1], tc.position[2]);
    const glm::vec3 player_position(ec.position[0], ec.position[1], ec.position[2]);

    const auto awareness = awareness_.state();
    const bool alert = awareness == Awareness::eAlert;
    // an alert enemy hunts the player it perceives (or did a moment ago); otherwise it goes where it last noticed the
    // player
    const bool hunting_player = awareness_.hunting(cfg.perception);
    const glm::vec3 target = hunting_player ? player_position : awareness_.last_known_position();
    bool goes_to_target = awareness != Awareness::eUnaware;
    const float move_speed = alert ? cfg.move_speed : cfg.perception.suspicious_move_speed;

    const bool player_out_of_reach = std::abs(ec.position[1] - tc.position[1]) > ENEMY_MAX_REACH_HEIGHT;
    // only a player it hunts is attacked
    const auto distance_to_player = player_out_of_reach || !hunting_player
        ? std::numeric_limits<float>::max()
        : glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(ec.position[0], ec.position[2]));
    const float distance_to_target = std::abs(target.y - tc.position[1]) > ENEMY_MAX_REACH_HEIGHT
        ? std::numeric_limits<float>::max()
        : glm::distance(glm::vec2(tc.position[0], tc.position[2]), glm::vec2(target.x, target.z));
    // a target out of its reach (on a roof, or while one of them falls) isn't walked to
    goes_to_target = goes_to_target && distance_to_target < std::numeric_limits<float>::max();

    // path[0] is the enemy's own position (on the nav mesh), path[1] the next corner to walk to; a target it can't reach
    // (a partial path) is not walked to
    std::array<glm::vec3, ENEMY_MAX_PATH_POINTS> path{};
    std::size_t path_points_count = 0;
    engine_nav_path_status_t path_status = ENGINE_NAV_PATH_STATUS_COMPLETE;
    if (!goes_to_target
        || distance_to_target < ENEMY_DIRECT_APPROACH_RANGE
        || engineNavFindPath(tc.position, glm::value_ptr(target), ENEMY_PATH_SEARCH_HALF_EXTENTS, glm::value_ptr(path[0]), path.size(), &path_points_count, &path_status) != ENGINE_RESULT_CODE_OK
        || path_status == ENGINE_NAV_PATH_STATUS_PARTIAL)
    {
        path_points_count = 0;
    }
    const bool has_path = path_points_count >= 2;
    // a hunted player is reached at attack range, a spot it walks to a little closer
    const float arrive_distance = hunting_player ? cfg.attack_range : ENEMY_ARRIVE_DISTANCE;

    if (hp <= 0 && state_ != States::DIE)
    {
        state_ = States::DIE;
        set_attack_window_active(false);
        if (!engineAnimationControllerLayerSetLooping(go_, 0, false))
        {
            engineLog("Failed to disable looping for enemy death.\n");
        }
        if (!engineAnimationControllerAnimationPlay(go_, "Death_A", 0))
        {
            engineLog("Failed to play enemy death animation.\n");
        }
        // remove its body so the enemy will not be hit by players attacks (nor block anyone)
        engineGameObjectRemoveCharacterControllerComponent(go_);
        if (reinterpret_cast<AppProjectC*>(my_scene_->get_app())->is_prefab_available(project_c::PrefabType::PREFAB_TYPE_COIN_GOLD))
        {
            auto coin = my_scene_->register_script<project_c::Coin>(reinterpret_cast<AppProjectC*>(my_scene_->get_app())->spawn_prefab(project_c::PREFAB_TYPE_COIN_GOLD).go);
            coin->set_world_position(tc.position[0], tc.position[1] + 1.0f, tc.position[2]);
        }
    }

    const auto* encounter = my_scene_->find_script<Encounter>();
    if (encounter && encounter->is_over() && state_ != States::DIE)
    {
        return;
    }

    // horizontal velocity of this frame; the character controller adds falling and collisions
    glm::vec3 velocity(0.0f);
    switch (state_)
    {
    case States::DECISION_MAKE:
    {
        const auto* attack = distance_to_player <= cfg.attack_range && attack_cooldown_remaining_seconds_ <= 0.0f ? next_attack(cfg) : nullptr;
        if (attack)
        {
            state_ = States::ATTACK;
            attack_animation_ = attack->animation;
            if (auto* blade = weapon())
            {
                blade->begin_swing();
            }
            if (!engineAnimationControllerLayerSetLooping(go_, 0, false))
            {
                engineLog("Failed to disable looping for enemy attack.\n");
            }
            if (!engineAnimationControllerIsAnimationPlaying(go_, attack_animation_.c_str()))
            {
                if (!engineAnimationControllerAnimationPlay(go_, attack_animation_.c_str(), 0))
                {
                    engineLog("Failed to start enemy attack animation.\n");
                    state_ = States::DECISION_MAKE;
                    attack_cooldown_remaining_seconds_ = cfg.attack_cooldown_seconds;
                }
            }
        }
        else if (goes_to_target && distance_to_target > arrive_distance && (has_path || distance_to_target < ENEMY_DIRECT_APPROACH_RANGE))
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
        if (!engineAnimationControllerLayerSetLooping(go_, 0, true))
        {
            engineLog("Failed to enable looping for enemy idle.\n");
        }
        if (!engineAnimationControllerIsAnimationPlaying(go_, "Idle"))
        {
            engineAnimationControllerAnimationPlay(go_, "Idle", 0);
        }
        // where the player was lost it looks around; a suspicious one faces what it noticed
        if (awareness == Awareness::eSearching)
        {
            const auto turn = glm::angleAxis(glm::radians(ENEMY_LOOK_AROUND_DEGREES_PER_SECOND) * dt / 1000.0f, glm::vec3(0.0f, 1.0f, 0.0f));
            const auto quat = turn * glm::make_quat(tc.rotation);
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            engineGameObjectUpdateTransformComponent(go_, &tc);
        }
        else if (awareness == Awareness::eSuspicious && distance_to_target > 0.05f && distance_to_target < std::numeric_limits<float>::max())
        {
            auto quat = utils::rotate_toward(position, glm::vec3(target.x, position.y, target.z));
            quat = glm::slerp(glm::make_quat(tc.rotation), quat, std::min(1.0f, 0.005f * dt));
            std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
            engineGameObjectUpdateTransformComponent(go_, &tc);
        }
        state_ = States::DECISION_MAKE;
        break;
    }
    case States::ATTACK:
    {
        if (!engineAnimationControllerIsAnimationPlaying(go_, attack_animation_.c_str()))
        {
            state_ = States::DECISION_MAKE;
            set_attack_window_active(false);
            attack_cooldown_remaining_seconds_ = cfg.attack_cooldown_seconds;
            attack_index_++;
        }

        auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
        quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
        std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
        engineGameObjectUpdateTransformComponent(go_, &tc);

        break;
    }
    case States::DIE:
    {
        if (!engineAnimationControllerIsAnimationPlaying(go_, "Death_A"))
        {
            my_scene_->unregister_script(this);
        }
        break;
    }
    case States::MOVE:
    {
        // path is computed per_frame, but decision was made frame before
        // so it can happen that target has moved and new position is not reachable or close to current position
        const bool direct_approach = !has_path && distance_to_target < ENEMY_DIRECT_APPROACH_RANGE;
        if (!goes_to_target || distance_to_player <= cfg.attack_range || distance_to_target <= arrive_distance || (!has_path && !direct_approach))
        {
            state_ = States::DECISION_MAKE;
            break;
        }
        if (!engineAnimationControllerLayerSetLooping(go_, 0, true))
        {
            engineLog("Failed to enable looping for enemy movement.\n");
        }
        // alert it runs, otherwise it walks
        const char* move_animation = alert ? "Running_A" : "Walking_A";
        if (!engineAnimationControllerIsAnimationPlaying(go_, move_animation))
        {
            engineAnimationControllerAnimationPlay(go_, move_animation, 0);
        }
        for (std::size_t i = 1; i < path_points_count; i++)
        {
            debug_scripts_.push_back(my_scene_->register_script<project_c::DebugPathNode>(engineGameObjectCreateTransient(), path[i].x, path[i].z));
        }
        const glm::vec3 next = direct_approach
            ? glm::vec3(target.x, tc.position[1], target.z)
            : path[1];
        auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), next);
        //auto quat = utils::rotate_toward(glm::vec3(tc.position[0], tc.position[1], tc.position[2]), glm::vec3(ec.position[0], ec.position[1], ec.position[2]));
        quat = glm::slerp(glm::make_quat(tc.rotation), quat, 0.005f * dt);
        std::memcpy(tc.rotation, glm::value_ptr(quat), sizeof(tc.rotation));
        engineGameObjectUpdateTransformComponent(go_, &tc);
        const glm::vec3 forward = glm::normalize(quat * glm::vec3(0.0f, 0.0f, 1.0f));
        velocity = glm::vec3(forward.x, 0.0f, forward.z) * move_speed;
        //state_ = States::DECISION_MAKE;
        break;
    }
    default:
    {
        engineLog("Unknown enemy state\n");
        break;
    }
    }
    if (state_ != States::DIE && engineCharacterControllerMove(go_, glm::value_ptr(velocity), dt, nullptr) != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed to move the enemy.\n");
    }
}

void project_c::Enemy::set_attack_window_active(bool active)
{
    if (auto* blade = weapon())
    {
        blade->set_attack_window_active(active);
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
        player->receive_damage(tuning().attack_damage, glm::vec3(tc.position[0], tc.position[1], tc.position[2]));
    }
}

void project_c::Enemy::receive_attack_hit(std::uint32_t damage, float hit_stop_seconds)
{
    // alerted at the next update, with where the player is now
    damaged_ = true;
    if (engineGameObjectIsValid(player_go_))
    {
        const auto ptc = engineGameObjectGetTransformComponent(player_go_);
        damage_position_ = glm::vec3(ptc.position[0], ptc.position[1], ptc.position[2]);
    }
    set_attack_window_active(false);
    hp = damage >= hp ? 0 : hp - damage;
    hit_flash_remaining_seconds_ = 0.12f;
    update_hit_flash(1.0f);

    hit_stop_remaining_seconds_ = std::max(hit_stop_remaining_seconds_, hit_stop_seconds);
    hit_stop_just_started_ = true;
    if (!engineAnimationControllerSetPaused(go_, true))
    {
        engineLog("Failed to pause enemy animation for hit-stop.\n");
    }
}

bool project_c::Enemy::was_just_killed() const
{
    return is_dying() && awareness_.state() == Awareness::eAlert && dying_seconds_ < ENEMY_DEATH_ALARM_SECONDS;
}

project_c::EnemyStimuli project_c::Enemy::perceive(const EnemyPerception& perception, const glm::vec3& position, const glm::vec3& forward) const
{
    EnemyStimuli stimuli{};
    stimuli.damaged = damaged_;
    stimuli.damage_position = damage_position_;

    // its pack: alert while another member perceived the player recently
    if (const auto* pack = my_scene_->get_script<EnemySpawner>(pack_))
    {
        const auto state = pack->pack_state(go_);
        if (state.alert && state.seconds_since_perceived < perception.lose_seconds)
        {
            stimuli.pack_alert = true;
            stimuli.pack_position = state.last_known_position;
        }
    }

    const auto* player = my_scene_->get_script<Player>(player_go_);
    if (!player || player->is_dead())
    {
        return stimuli;
    }
    const auto ptc = engineGameObjectGetTransformComponent(player_go_);
    const glm::vec3 player_position(ptc.position[0], ptc.position[1], ptc.position[2]);
    const float distance = glm::distance(glm::vec2(position.x, position.z), glm::vec2(player_position.x, player_position.z));

    // sight: in view, and nothing in the way from its eyes to the player's chest
    if (in_sight_cone(position, forward, player_position, perception))
    {
        const glm::vec3 eye = position + glm::vec3(0.0f, perception.eye_height, 0.0f);
        const glm::vec3 chest = player_position + glm::vec3(0.0f, PLAYER_CHEST_HEIGHT, 0.0f);
        const glm::vec3 direction = chest - eye;
        engine_physics_query_filter_t filter{};
        filter.ignore = &go_;
        filter.ignore_count = 1;
        engine_ray_hit_t hit{};
        if (glm::length(direction) > 0.0f
            && enginePhysicsRayCast(glm::value_ptr(eye), glm::value_ptr(direction), glm::length(direction), &filter, &hit)
            && hit.go == player_go_)
        {
            stimuli.seen = true;
            stimuli.seen_distance = distance;
            stimuli.seen_above = player_position.y - position.y > ENEMY_MAX_REACH_HEIGHT;
            stimuli.seen_position = player_position;
        }
    }

    // hearing: the player's noise of this frame reaches it (walls don't block it)
    const float noise = player->noise_radius();
    if (noise > 0.0f && glm::distance(position, player_position) <= noise)
    {
        if (player->noise_is_loud())
        {
            stimuli.heard_loud_noise = true;
            stimuli.loud_noise_position = player_position;
        }
        else
        {
            stimuli.heard_footsteps = true;
            stimuli.footsteps_position = player_position;
        }
    }
    return stimuli;
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
        HitFlashMaterial entry{};
        entry.game_object = game_object;
        std::copy(
            std::begin(material.diffuse_color),
            std::end(material.diffuse_color),
            entry.original_diffuse_color.begin());
        hit_flash_materials_.push_back(entry);
    }

    for (const auto child : utils::get_children(game_object))
    {
        collect_hit_flash_materials(child);
    }
}

void project_c::Enemy::update_hit_flash(float strength)
{
    const auto cfg = tuning();
    const auto& hit_color = cfg.hit_flash_color;
    strength = std::clamp(strength, 0.0f, 1.0f);

    for (const auto& entry : hit_flash_materials_)
    {
        if (!engineGameObjectIsValid(entry.game_object) || !engineGameObjectHasMaterialComponent(entry.game_object))
        {
            continue;
        }

        auto material = engineGameObjectGetMaterialComponent(entry.game_object);
        for (std::size_t channel = 0; channel < std::size(hit_color); ++channel)
        {
            const float original = entry.original_diffuse_color[channel];
            material.diffuse_color[channel] = original + (hit_color[channel] - original) * strength;
        }
        engineGameObjectUpdateMaterialComponent(entry.game_object, &material);
    }
}
