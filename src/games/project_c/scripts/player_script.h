#pragma once
#include "base_script.h"
#include "game_components.h"
#include "player_traversal.h"
#include "../player_input.h"
#include "../player_state.h"

#include <array>
#include <optional>
#include <string>
#include <chrono>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace project_c
{
class AppProjectC;
class Player;

class AttackTrigger : public BaseNode
{
public:
    // `player`: the game object of the Player it belongs to
    AttackTrigger(engine::IScene* my_scene, engine_game_object_t go, engine_game_object_t player);

    void begin_swing();
    void activate();
    void deactivate();
    void on_collision(const collision_t& info) override;

private:
    bool is_active_ = false;
    // its Player is looked up on each hit: the script can go while its game object stays
    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    std::unordered_set<engine_game_object_t> hit_enemies_;
};

class Weapon : public BaseNode
{
public:
    Weapon(engine::IScene* my_scene, engine_game_object_t go);
    ~Weapon();

    // what a weapon is made of (name, transform, the rogue's dagger mesh and material, a trigger collider), for the
    // level's weapons and the ones the player spawns
    static void add_components(engine_game_object_t go, const AppProjectC& app);
    // the components of a weapon lying on the ground at `position`: dropped from above it as a dynamic body
    static void add_lying_components(engine_game_object_t go, const glm::vec3& position);

    void on_start() override;

    void attach_to_game_object(engine_game_object_t parent, std::string_view joint_name, std::optional<glm::vec3> position, std::optional<glm::quat> rotation);
    void attach_offhand_to_game_object(engine_game_object_t parent, std::string_view joint_name);
    void drop_on_ground(glm::vec3 position);

    void on_collision(const collision_t& info) override;

    // lying on the ground (can be picked up), not held by anyone
    bool is_on_ground() const;
    // what the player carries when holding it (every weapon is daggers for now)
    WeaponType get_type() const { return WeaponType::eDaggers; }

private:
    struct OffhandHolder
    {
        engine_game_object_t parent;
        std::string joint_name;
    };

    // second dagger: visual only, follows main dagger when it is not held; created in on_start (a runtime object)
    engine_game_object_t offhand_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    // where the second dagger is held, nullopt when it lies next to the main dagger
    std::optional<OffhandHolder> offhand_holder_;

    void attach_offhand_to_main();
    void place_offhand();
};

class Player : public BaseNode
{
private:
    enum States : std::uint32_t
    {
        IDLE   = 0x0000,
        ATTACK = 0x0002,
        DEAD   = 0x0004
    };

    struct GlobalStateData
    {
        engine_ray_hit_t last_mouse_hit = {};
    };

    struct MoveStateData
    {
        enum class Direction
        {
            eForward,
            eBackward,
            eLeft,
            eRight
        };

        const char* direction_to_string(Direction dir) const;
        const char* get_animation_name(Direction dir) const;
        float get_animation_reference_speed(Direction dir) const;
    };

    struct AttackStateData
    {
        // all times are in seconds of the animation timeline
        struct HitWindow
        {
            float start;
            float end;
        };

        struct Attack
        {
            const char* animation_name;
            std::array<HitWindow, 2> hit_windows;
            std::size_t hit_windows_count;
            float combo_window_start;
            float combo_window_end;
            float exit_time; // attack can be left at this point, even if animation still plays its recovery
        };

        static constexpr std::size_t combo_length = 3;
        const Attack& get_attack(std::size_t combo_index) const;
        const char* get_animation_name(std::size_t combo_index) const;
    };

    // Currently informational only. 
    // ToDo: For example could allow turning during windup or dodging during recovery.
    enum class AttackPhase
    {
        eIdle,
        eWindup,
        eActive,
        eRecovery
    };

public:
    // the scale of the player's model in the level
    static constexpr float MODEL_SCALE = 0.35f;

    Player(engine::IScene* my_scene, engine_game_object_t go);
    // removes its animation events and the runtime objects it spawned (also when its script component is removed and
    // its game object stays)
    ~Player();

    // its tuning (the player_tuning data asset its player_config component references), read on every use
    PlayerTuning tuning() const;
    // its traversal tuning (the traversal_tuning data asset of its player_config), read on every use
    TraversalTuning traversal_tuning() const;

    void on_start() override;
    void update(float dt);
    void on_confirmed_attack_hit(float hit_stop_seconds);
    bool receive_damage(std::uint32_t damage, const glm::vec3& source_position);
    std::uint32_t get_hp() const { return state().hp; }
    std::uint32_t get_max_hp() const { return tuning().max_hp; }
    bool is_dead() const { return state().hp == 0; }

    bool equip_waepon(Weapon* sword);
    // the hit window of the current swing opened or closed (animation events)
    void set_attack_window_active(bool active);

    void add_coin(std::uint64_t amount);
    std::uint64_t get_coins() const { return state().coins; }
    MovementMode movement_mode() const { return movement_mode_; }
    // The noise the player makes this frame, which enemies within the radius hear (walls don't block it): 0 while
    // sneaking, standing or off the ground, the footstep radius while walking, the loud radius for one frame on an
    // attack, a dodge or a hard landing (noise_is_loud).
    float noise_radius() const { return noise_radius_; }
    bool noise_is_loud() const { return noise_is_loud_; }
    // its stats (hp, coins, weapon), kept by the app so they outlive the level (AppProjectC::player_state)
    const PlayerState& state() const;
    // what a player who has nothing carried starts with
    PlayerState fresh_state() const;

private:
    // this frame's player input (AppProjectC::player_input), never the keyboard/mouse directly
    const PlayerInput& input() const;
    // state(), to change it
    PlayerState& edit_state();

    enum LayerIDs : std::size_t
    {
        LOCOMOTION_LAYER_ID = 0,
        COMBAT_LAYER_ID = 1
    };

    std::uint32_t state_;
    MoveStateData move_data_;
    AttackStateData attack_data_;
    GlobalStateData global_data_;
    glm::vec2 movement_velocity_{ 0.0f };
    std::size_t attack_combo_index_ = 0;
    float attack_elapsed_ = 0.0f;
    float attack_buffer_remaining_ = 0.0f;
    bool attack_buffered_ = false;
    bool attack_press_pending_ = false;
    bool drop_weapon_press_pending_ = false;
    AttackPhase attack_phase_ = AttackPhase::eIdle;
    bool combat_layer_has_pose_ = false;
    float hit_stop_remaining_seconds_ = 0.0f;
    bool hit_stop_just_started_ = false;
    bool dodge_press_pending_ = false;
    float dodge_remaining_seconds_ = 0.0f;
    float dodge_invulnerability_remaining_seconds_ = 0.0f;
    float dodge_cooldown_remaining_seconds_ = 0.0f;
    float hit_stun_remaining_seconds_ = 0.0f;
    glm::vec3 dodge_direction_{ 0.0f, 0.0f, 1.0f };
    glm::vec3 knockback_velocity_{ 0.0f };

    // Its attack trigger and its weapon by game object, their scripts looked up on use: a script can go while its game
    // object stays (its script component removed in the editor), so no pointer to another script is kept.
    AttackTrigger* attack_trigger() const;
    Weapon* weapon() const;
    engine_game_object_t attack_trigger_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    // the animation events calling it (removed with it), by animation
    std::vector<std::pair<const char*, engine_animation_event_id_t>> animation_events_;

    // inventory
    // the held weapon's runtime object (what it holds is state().weapon)
    engine_game_object_t weapon_go_ = ENGINE_INVALID_GAME_OBJECT_ID;

    bool start_attack(std::size_t combo_index);
    bool start_dodge(const glm::vec2& move_input, const glm::quat& rotation);

    // how the player moves now; changed only by set_movement_mode
    MovementMode movement_mode_ = MovementMode::eGrounded;
    // the transitions between the modes: what leaving one and entering the next changes (the combat layer, the
    // locomotion layer's settings)
    void set_movement_mode(MovementMode mode, const TraversalTuning& traversal);
    // the attack, dodge and stun in progress end (the player leaves the ground)
    void cancel_combat_actions();
    // the horizontal velocity the character controller moved the player with in the last grounded frame
    glm::vec3 ground_velocity_{ 0.0f };
    // how long the character controller hasn't stood the player on the ground
    float ungrounded_seconds_ = 0.0f;
    float landing_recovery_remaining_seconds_ = 0.0f;
    // the feet's height when the player left the ground, to tell a hard landing
    float airborne_start_y_ = 0.0f;

    // this frame's noise (noise_radius), and the ring shown on the ground for a loud one
    void make_loud_noise(const PlayerTuning& t);
    float noise_radius_ = 0.0f;
    bool noise_is_loud_ = false;
    glm::vec3 noise_ring_center_{ 0.0f };
    float noise_ring_radius_ = 0.0f;
    float noise_ring_remaining_seconds_ = 0.0f;
    PlayerTraversal traversal_;
};
} //namespace project_c