#pragma once
#include "base_script.h"
#include "enemy_awareness.h"
#include "fall_damage.h"
#include "game_components.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace project_c
{
class Enemy;

class EnemyWeapon : public BaseNode
{
public:
    // `enemy`: the game object of the Enemy holding it
    EnemyWeapon(engine::IScene* my_scene, engine_game_object_t go, engine_game_object_t enemy);

    void begin_swing();
    void set_attack_window_active(bool active);
    void on_collision(const collision_t& info) override;

private:
    // its Enemy is looked up on each hit: the script can go while its game object stays
    engine_game_object_t enemy_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    bool attack_window_active_ = false;
    bool hit_player_this_swing_ = false;
};

class Enemy : public BaseNode
{
private:
    enum class States
    {
        DECISION_MAKE = 0,
        IDLE = 1,
        ATTACK,
        DIE,
        MOVE,
        // knocked down by a plunge: lies, then stands up; it neither moves nor attacks
        KNOCKED_DOWN,
        // off the ground (kicked off a ledge): no AI, it keeps the push it had; landing hurts (fall damage)
        FALLING,
        // kicked: pushed back, it can neither move nor attack for a moment
        STAGGERED
    };

    struct IdleStateData {};
    struct DyingStateData {};
    struct MoveStateData {};

public:
    // the scale of the enemy's model in the level
    static constexpr float MODEL_SCALE = 0.35f;

    // set to the config's max_hp when the enemy starts
    std::uint32_t hp = 0;

    Enemy(engine::IScene* my_scene, engine_game_object_t go);
    // removes its animation events and its blade (also when its script component is removed and its game object stays)
    ~Enemy();

    // the tuning of its kind (the enemy_type data asset its enemy_config component references), read on every use
    EnemyType tuning() const;
    std::uint32_t get_max_hp() const { return tuning().max_hp; }

    void on_start() override;

    void update(float dt);
    // a hit by the player: it and its pack are alerted at once
    void receive_attack_hit(std::uint32_t damage, float hit_stop_seconds);
    // The player's execution starts on it: it is held where it is (no moving, turning or attacking) until
    // receive_execution.
    void begin_execution();
    bool is_being_executed() const { return being_executed_; }
    // The execution's strike: it dies silently (Death_B; its death doesn't alert the pack) when its type says an
    // execution kills it, otherwise it takes `damage` and is alerted like by a hit.
    void receive_execution(std::uint32_t damage);
    // a plunge landed next to it: it lies for `seconds` and stands up, alerted
    void knock_down(float seconds);
    bool is_knocked_down() const { return state_ == States::KNOCKED_DOWN && knocked_down_remaining_seconds_ > 0.0f; }
    // off the ground, falling (no AI until it lands)
    bool is_falling() const { return state_ == States::FALLING; }
    // The player's kick: pushed along `direction` (flat, normalized) at `speed` metres per second (the push decays; off a
    // ledge it falls), staggered for `stagger_seconds` (not while it lies knocked down: it only takes the push) and hurt
    // by `damage`. It is alerted once it recovers or lands, unless the fall kills it.
    void receive_shove(const glm::vec3& direction, float speed, float stagger_seconds, std::uint32_t damage);
    bool is_staggered() const { return state_ == States::STAGGERED; }
    // a body landed from a fall this frame: a loud noise where it landed, heard by the enemies around
    bool landing_noise(glm::vec3& position) const
    {
        position = landing_noise_position_;
        return landing_noise_;
    }
    void set_attack_window_active(bool active);
    void on_weapon_hit_player(engine_game_object_t player_go);

    // what it knows of the player (its detection meter and state)
    const EnemyAwareness& awareness() const { return awareness_; }
    // killed (its death plays): it is no longer part of its pack, except to alert it right after the killing blow
    bool is_dying() const { return hp == 0; }
    // killed a moment ago by a hit: its pack takes the alert (the hit's position is its last known position)
    bool was_just_killed() const;
    // Its pack: the EnemySpawner which spawned it (runtime state, never saved), whose members alert each other;
    // ENGINE_INVALID_GAME_OBJECT_ID for a placed enemy, a pack of one (and so is one whose spawner is gone).
    void set_pack(engine_game_object_t spawner) { pack_ = spawner; }
    engine_game_object_t get_pack() const { return pack_; }

private:
    // the player's game object, or none (a scene without a player, or the player is gone)
    void find_player();
    // what it sees, hears and was told by its pack this frame, standing at `position` facing `forward`
    EnemyStimuli perceive(const EnemyPerception& perception, const glm::vec3& position, const glm::vec3& forward) const;
    // the ray from its eyes to the player's chest reaches the player: only level geometry hides it, not other
    // characters nor dynamic bodies
    bool sees(const glm::vec3& eye, const glm::vec3& to_player) const;

    struct HitFlashMaterial
    {
        engine_game_object_t game_object;
        std::array<float, 4> original_diffuse_color;
    };

    void collect_hit_flash_materials(engine_game_object_t game_object);
    void update_hit_flash(float strength);

    bool triggered_ = false;
    bool attack_right_ = false;
    States state_;
    // the type's attack used next (skipping the ones without an animation), and the animation of the one playing
    std::uint32_t attack_index_ = 0;
    std::string attack_animation_;
    std::vector<IScript*> debug_scripts_;
    std::vector<HitFlashMaterial> hit_flash_materials_;
    // the blade by game object, its script looked up on use: a script can go while its game object stays (its script
    // component removed in the editor), so no pointer to another script is kept
    EnemyWeapon* weapon() const;
    engine_game_object_t weapon_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    // the type's attack, or nullptr when it has none
    const EnemyAttack* next_attack(const EnemyType& type);
    // the animation events opening and closing the attack windows, made again when the type's attacks change
    void sync_attack_events(const EnemyType& type);
    void remove_attack_events();
    // the animation events calling it (removed with it), by animation, and the attacks they were made for
    std::vector<std::pair<std::string, engine_animation_event_id_t>> animation_events_;
    EnemyAttack attack_events_for_[ENEMY_TYPE_MAX_ATTACKS]{};
    bool attack_events_made_ = false;
    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;  // found in on_start, again when it is gone
    float hit_flash_remaining_seconds_ = 0.0f;
    float hit_stop_remaining_seconds_ = 0.0f;
    float attack_cooldown_remaining_seconds_ = 0.0f;
    bool hit_stop_just_started_ = false;

    EnemyAwareness awareness_;
    engine_game_object_t pack_ = ENGINE_INVALID_GAME_OBJECT_ID;
    // a hit since the last update, and where the player was then
    bool damaged_ = false;
    glm::vec3 damage_position_{ 0.0f };
    // the player's position now, as the last known position of a hit
    void note_damage_position();
    bool being_executed_ = false;
    // killed by an execution: silently
    bool executed_ = false;
    std::string death_animation_ = "Death_A";
    float knocked_down_remaining_seconds_ = 0.0f;
    bool standing_up_ = false;
    float dying_seconds_ = 0.0f;

    // horizontal push (a kick), decaying; it moves the enemy with its character controller, also off ledges
    glm::vec3 knockback_velocity_{ 0.0f };
    float stagger_remaining_seconds_ = 0.0f;
    // kicked and not alerted yet: it is once it recovers or survives its landing (a fall death stays quiet for its pack)
    bool alert_after_shove_ = false;
    // seconds the controller has had nothing under it (a step down isn't a fall)
    float ungrounded_seconds_ = 0.0f;
    FallMeasure fall_;
    // falls: when the controller leaves the ground for long enough, and the landing (damage, a knockdown or death)
    void start_falling(float height);
    void land(const EnemyType& type, float height);
    // the noise of its landing, heard during one update of each enemy (kept until its own next update)
    bool landing_noise_ = false;
    glm::vec3 landing_noise_position_{ 0.0f };
    void set_landing_noise(bool on, const glm::vec3& position = glm::vec3(0.0f));
    // how many enemies have a landing noise now: the others look for them only then
    static inline std::uint32_t landing_noises_ = 0;
};
} //namespace project_c