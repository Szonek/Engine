#include "enemy_script.h"
#include "smoke_cloud.h"
#include "game_layers.h"
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
#include <utility>
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
// a kick's push decays like the player's knockback (exponentially), more slowly in the air
constexpr float ENEMY_KNOCKBACK_DECAY = 4.0f;
constexpr float ENEMY_AIR_DRAG = 0.5f;
// seconds the character controller has nothing under the enemy before it falls (a step down isn't a fall)
constexpr float ENEMY_FALL_DELAY_SECONDS = 0.1f;
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
    set_layer(cc, Layer::eHitbox);
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
    set_landing_noise(false);
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

    // its landing noise of the last frame has been heard by every other enemy since
    set_landing_noise(false);

    // what it notices of the player (also when there is none: it forgets), then (after a hit stop) what it does about
    // it: a hit alerts it at once, also during the hit stop. A player destroyed since (e.g. deleted in the editor) is
    // looked for again.
    if (!engineGameObjectIsValid(player_go_))
    {
        find_player();
    }
    // also for the killing blow (it isn't dying yet), so the hit alerts it and its pack; not while the player executes
    // it (from right behind it, in its close range): an execution is silent
    if (state_ != States::DIE && !being_executed_)
    {
        const auto own = engineGameObjectGetTransformComponent(go_);
        const glm::vec3 forward = glm::make_quat(own.rotation) * glm::vec3(0.0f, 0.0f, 1.0f);
        awareness_.update(dt / 1000.0f, perceive(cfg.perception, glm::vec3(own.position[0], own.position[1], own.position[2]), forward), cfg.perception);
    }
    damaged_ = false;
    pending_loud_noise_ = false;
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
            // frozen by the hit stop, but a body in the air (or pushed) keeps moving
            if (moves_without_ai())
            {
                move_body(cfg, glm::vec3(0.0f), dt);
            }
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

    // a scene without a player (e.g. a test scene), or the player was destroyed (e.g. deleted in the editor): no AI,
    // but a body in the air (or pushed) keeps moving
    if (!engineGameObjectIsValid(player_go_))
    {
        if (moves_without_ai())
        {
            move_body(cfg, glm::vec3(0.0f), dt);
        }
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
        if (!engineAnimationControllerAnimationPlay(go_, death_animation_.c_str(), 0))
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
    // the encounter is over (no AI), or the player's execution holds it: a body in the air (or pushed) keeps moving
    if ((encounter && encounter->is_over() || being_executed_) && state_ != States::DIE)
    {
        if (moves_without_ai())
        {
            move_body(cfg, glm::vec3(0.0f), dt);
        }
        return;
    }

    // horizontal velocity of this frame; the character controller adds falling and collisions
    glm::vec3 velocity(0.0f);
    switch (state_)
    {
    case States::BLINDED:
    {
        // it stands, seeing nothing: the hit reaction, then idle, then it searches (from where it lost the player)
        blinded_remaining_seconds_ = std::max(0.0f, blinded_remaining_seconds_ - dt / 1000.0f);
        if (blinded_remaining_seconds_ <= 0.0f)
        {
            state_ = States::DECISION_MAKE;
            break;
        }
        if (!engineAnimationControllerIsAnimationPlaying(go_, "Hit_B") && !engineAnimationControllerIsAnimationPlaying(go_, "Idle"))
        {
            engineAnimationControllerLayerSetLooping(go_, 0, true);
            engineAnimationControllerAnimationPlay(go_, "Idle", 0);
        }
        break;
    }
    case States::STAGGERED:
    {
        // pushed back, it can't act; then it is alerted (it knows who kicked it)
        stagger_remaining_seconds_ = std::max(0.0f, stagger_remaining_seconds_ - dt / 1000.0f);
        if (stagger_remaining_seconds_ <= 0.0f)
        {
            if (std::exchange(alert_after_shove_, false))
            {
                note_damage_position();
            }
            state_ = States::DECISION_MAKE;
        }
        break;
    }
    case States::FALLING:
    {
        // no AI in the air: it keeps the push it had, slowed a little by the air
        if (!engineAnimationControllerIsAnimationPlaying(go_, "Jump_Idle"))
        {
            engineAnimationControllerLayerSetLooping(go_, 0, true);
            engineAnimationControllerAnimationPlay(go_, "Jump_Idle", 0);
        }
        break;
    }
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
    case States::KNOCKED_DOWN:
    {
        knocked_down_remaining_seconds_ = std::max(0.0f, knocked_down_remaining_seconds_ - dt / 1000.0f);
        if (knocked_down_remaining_seconds_ > 0.0f)
        {
            break;
        }
        if (!standing_up_)
        {
            standing_up_ = true;
            if (!engineAnimationControllerAnimationPlay(go_, "Lie_StandUp", 0))
            {
                engineLog("Failed to play the enemy's stand up animation.\n");
            }
        }
        else if (!engineAnimationControllerIsAnimationPlaying(go_, "Lie_StandUp"))
        {
            standing_up_ = false;
            state_ = States::DECISION_MAKE;
        }
        break;
    }
    case States::DIE:
    {
        if (!engineAnimationControllerIsAnimationPlaying(go_, death_animation_.c_str()))
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
    if (state_ == States::DIE)
    {
        return;
    }
    move_body(cfg, velocity, dt);
}

bool project_c::Enemy::moves_without_ai() const
{
    return state_ == States::FALLING || ungrounded_seconds_ > 0.0f || glm::dot(knockback_velocity_, knockback_velocity_) > 0.0f;
}

void project_c::Enemy::move_body(const EnemyType& cfg, glm::vec3 velocity, float dt)
{
    // the push of a kick adds to its own move, and decays (more slowly in the air)
    const float delta_seconds = dt / 1000.0f;
    velocity += knockback_velocity_;
    knockback_velocity_ *= std::exp(-(state_ == States::FALLING ? ENEMY_AIR_DRAG : ENEMY_KNOCKBACK_DECAY) * delta_seconds);
    engine_character_controller_move_result_t moved{};
    if (engineCharacterControllerMove(go_, glm::value_ptr(velocity), dt, &moved) != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed to move the enemy.\n");
        return;
    }
    const float height = engineGameObjectGetTransformComponent(go_).position[1];
    const bool supported = moved.grounded || moved.blocked_below;
    if (state_ == States::FALLING)
    {
        fall_.update(height);
        if (supported)
        {
            land(cfg, height);
        }
        return;
    }
    // nothing under it for a moment (not a step down): it falls, from where it left the ground
    if (supported)
    {
        ungrounded_seconds_ = 0.0f;
        return;
    }
    if (ungrounded_seconds_ <= 0.0f)
    {
        fall_.start(height);
    }
    fall_.update(height);
    ungrounded_seconds_ += delta_seconds;
    if (ungrounded_seconds_ >= ENEMY_FALL_DELAY_SECONDS)
    {
        start_falling(height);
    }
}

void project_c::Enemy::start_falling(float /*height*/)
{
    // what it was doing ends; it neither attacks nor walks in the air
    set_attack_window_active(false);
    standing_up_ = false;
    knocked_down_remaining_seconds_ = 0.0f;
    state_ = States::FALLING;
    engineAnimationControllerLayerSetLooping(go_, 0, true);
    engineAnimationControllerAnimationPlay(go_, "Jump_Idle", 0);
}

void project_c::Enemy::land(const EnemyType& type, float height)
{
    ungrounded_seconds_ = 0.0f;
    knockback_velocity_ = glm::vec3(0.0f);
    // kicked into this fall: whatever the landing, the kick's pending alert is settled here
    const bool kicked = std::exchange(alert_after_shove_, false);
    const float drop = fall_.land(height);
    const float fraction = fall_damage_fraction(type.fall, drop, false);
    if (fraction <= 0.0f)
    {
        // a drop it takes in its stride (a step, being spawned just above the floor): it goes on, alerted by a kick
        if (kicked)
        {
            note_damage_position();
        }
        state_ = States::DECISION_MAKE;
        return;
    }
    const auto own = engineGameObjectGetTransformComponent(go_);
    // a body hitting the ground: loud, the enemies around hear it
    set_landing_noise(true, glm::vec3(own.position[0], own.position[1], own.position[2]));
    const auto damage = static_cast<std::uint32_t>(std::ceil(fraction * static_cast<float>(type.max_hp)));
    if (damage > 0)
    {
        hp = damage >= hp ? 0 : hp - damage;
        hit_flash_remaining_seconds_ = 0.12f;
        update_hit_flash(1.0f);
    }
    if (hp == 0)
    {
        // it dies (the death plays next update), quietly for its pack; a kill by the player's kick fills its momentum
        if (kicked)
        {
            if (auto* player = my_scene_->get_script<Player>(player_go_))
            {
                player->on_kicked_enemy_fell_dead();
            }
        }
        state_ = States::DECISION_MAKE;
        return;
    }
    // it survives the landing: it lies a moment and stands up, alerted (knock_down alerts it)
    const auto* player = my_scene_->get_script<Player>(player_go_);
    knock_down(player ? player->execution_tuning().knockdown_seconds : default_execution_tuning().knockdown_seconds);
}

void project_c::Enemy::receive_shove(const glm::vec3& direction, float speed, float stagger_seconds, std::uint32_t damage)
{
    if (hp == 0 || being_executed_)
    {
        return;
    }
    hp = damage >= hp ? 0 : hp - damage;
    if (damage > 0)
    {
        hit_flash_remaining_seconds_ = 0.12f;
        update_hit_flash(1.0f);
    }
    knockback_velocity_ = glm::vec3(direction.x, 0.0f, direction.z) * speed;
    if (hp == 0)
    {
        // the kick itself killed it: an ordinary killing blow, which alerts its pack
        note_damage_position();
        return;
    }
    alert_after_shove_ = true;
    // lying (knocked down) it only takes the push: it stands up as it would have
    if (state_ == States::KNOCKED_DOWN)
    {
        return;
    }
    set_attack_window_active(false);
    standing_up_ = false;
    stagger_remaining_seconds_ = stagger_seconds;
    // already in the air it keeps falling (the push adds to it)
    if (state_ != States::FALLING)
    {
        state_ = States::STAGGERED;
        engineAnimationControllerLayerSetLooping(go_, 0, false);
        engineAnimationControllerAnimationPlay(go_, "Hit_A", 0);
    }
}

void project_c::Enemy::blind(float seconds, const glm::vec3& smoke_centre)
{
    awareness_.lose_track(smoke_centre);
    if (hp == 0 || being_executed_ || state_ == States::FALLING || state_ == States::KNOCKED_DOWN)
    {
        return;
    }
    set_attack_window_active(false);
    standing_up_ = false;
    stagger_remaining_seconds_ = 0.0f;
    blinded_remaining_seconds_ = seconds;
    state_ = States::BLINDED;
    engineAnimationControllerLayerSetLooping(go_, 0, false);
    engineAnimationControllerAnimationPlay(go_, "Hit_B", 0);
}

void project_c::Enemy::end_blindness()
{
    if (state_ == States::BLINDED)
    {
        blinded_remaining_seconds_ = 0.0f;
        state_ = States::DECISION_MAKE;
    }
}

void project_c::Enemy::hear_loud_noise(const glm::vec3& position)
{
    pending_loud_noise_ = true;
    pending_loud_noise_position_ = position;
}

void project_c::Enemy::set_landing_noise(bool on, const glm::vec3& position)
{
    if (on != landing_noise_)
    {
        landing_noises_ = on ? landing_noises_ + 1 : landing_noises_ - 1;
    }
    landing_noise_ = on;
    landing_noise_position_ = position;
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

void project_c::Enemy::note_damage_position()
{
    // alerted at the next update, with where the player is now
    damaged_ = true;
    if (engineGameObjectIsValid(player_go_))
    {
        const auto ptc = engineGameObjectGetTransformComponent(player_go_);
        damage_position_ = glm::vec3(ptc.position[0], ptc.position[1], ptc.position[2]);
    }
}

void project_c::Enemy::begin_execution()
{
    being_executed_ = true;
    set_attack_window_active(false);
    if (state_ == States::ATTACK || state_ == States::MOVE)
    {
        state_ = States::DECISION_MAKE;
    }
}

void project_c::Enemy::receive_execution(std::uint32_t damage)
{
    being_executed_ = false;
    set_attack_window_active(false);
    hit_flash_remaining_seconds_ = 0.12f;
    update_hit_flash(1.0f);
    if (tuning().execution_kills)
    {
        // silently: no alert of its own, so its death doesn't alert its pack
        hp = 0;
        executed_ = true;
        death_animation_ = "Death_B";
        return;
    }
    // a blow it survives: like a hit, it ends the smoke's blindness and alerts it
    end_blindness();
    note_damage_position();
    hp = damage >= hp ? 0 : hp - damage;
}

void project_c::Enemy::knock_down(float seconds)
{
    if (hp == 0)
    {
        return;
    }
    note_damage_position();
    set_attack_window_active(false);
    state_ = States::KNOCKED_DOWN;
    knocked_down_remaining_seconds_ = seconds;
    standing_up_ = false;
    if (!engineAnimationControllerLayerSetLooping(go_, 0, false) || !engineAnimationControllerAnimationPlay(go_, "Lie_Down", 0))
    {
        engineLog("Failed to play the enemy's knockdown animation.\n");
    }
}

void project_c::Enemy::receive_attack_hit(std::uint32_t damage, float hit_stop_seconds)
{
    note_damage_position();
    // a hit ends the smoke's blindness (it is alerted by the hit)
    end_blindness();
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

bool project_c::Enemy::sees(const glm::vec3& eye, const glm::vec3& to_player) const
{
    // level geometry and smoke clouds hide the player (other enemies, on the characters' layer, don't)
    return !utils::level_blocks(eye, eye + to_player) && !utils::smoke_blocks(*my_scene_, eye, eye + to_player);
}

bool project_c::Enemy::was_just_killed() const
{
    // an execution kills silently
    return is_dying() && !executed_ && awareness_.state() == Awareness::eAlert && dying_seconds_ < ENEMY_DEATH_ALARM_SECONDS;
}

project_c::EnemyStimuli project_c::Enemy::perceive(const EnemyPerception& perception, const glm::vec3& position, const glm::vec3& forward) const
{
    EnemyStimuli stimuli{};
    stimuli.damaged = damaged_;
    stimuli.damage_position = damage_position_;
    // blinded by smoke it perceives nothing, its pack neither (a mate seeing the player would undo the lost track at
    // once); only a hit reaches it, which ends the blindness. Its pack re-alerts it after the blindness, as usual.
    if (state_ == States::BLINDED)
    {
        return stimuli;
    }

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
        if (glm::length(direction) > 0.0f && sees(eye, direction))
        {
            stimuli.seen = true;
            stimuli.seen_distance = distance;
            stimuli.seen_above = player_position.y - position.y > ENEMY_MAX_REACH_HEIGHT;
            stimuli.seen_position = player_position;
        }
    }

    // hearing: a loud noise made at it (a smoke bomb's pop)
    if (pending_loud_noise_)
    {
        stimuli.heard_loud_noise = true;
        stimuli.loud_noise_position = pending_loud_noise_position_;
    }

    // hearing: a body landing near it is a loud noise (only looked for while one landed)
    if (landing_noises_ > 0)
    {
        const float radius = player->tuning().loud_noise_radius;
        for (const auto* other : my_scene_->get_scripts<Enemy>())
        {
            glm::vec3 landed{};
            if (other != this && other->landing_noise(landed) && glm::distance(position, landed) <= radius)
            {
                stimuli.heard_loud_noise = true;
                stimuli.loud_noise_position = landed;
            }
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
