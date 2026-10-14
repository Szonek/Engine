#pragma
#include "base_script.h"

#include <array>
#include <chrono>
#include <unordered_set>

#include <glm/glm.hpp>

namespace project_c
{
class Player;

class AttackTrigger : public BaseNode
{
public:
    AttackTrigger(engine::IScene* my_scene, engine_game_object_t go, Player* player);

    void begin_swing();
    void activate();
    void deactivate();
    void on_collision(const collision_t& info) override;

private:
    bool is_active_ = false;
    Player* player_ = nullptr;
    std::unordered_set<engine_game_object_t> hit_enemies_;
};

class Weapon : public BaseNode
{
public:
    Weapon(engine::IScene* my_scene);
    ~Weapon();

    void attach_to_game_object(engine_game_object_t parent, std::string_view joint_name, std::optional<glm::vec3> position, std::optional<glm::quat> rotation);
    void attach_offhand_to_game_object(engine_game_object_t parent, std::string_view joint_name);
    void drop_on_ground(glm::vec3 position);

    void on_collision(const collision_t& info) override;

    void update(float dt) override;

private:
    // second dagger: visual only, follows main dagger when it is not held
    engine_game_object_t offhand_go_ = ENGINE_INVALID_GAME_OBJECT_ID;

    void attach_offhand_to_main();
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
        engine_ray_hit_info_t last_mouse_hit = {};
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

    // Environment traversal (climbing walls, vaulting through windows, mantling onto roofs).
    // While traversing the player is kinematic: rigid body is removed so gravity does not fight the scripted motion.
    enum class TraversalState
    {
        eNone,
        eClimbing,      // attached to a wall, WASD moves on the wall
        eScriptedMove   // vault / mantle / climb down transitions, follows a fixed path
    };

    struct TraversalMove
    {
        glm::vec3 p0{ 0.0f };
        glm::vec3 p1{ 0.0f };
        glm::vec3 p2{ 0.0f };
        float duration = 0.0f;
        float elapsed = 0.0f;
        bool curved = false;        // quadratic curve through p1 (vault), otherwise two linear segments (mantle, climb down)
        float split = 0.5f;         // linear segments only: fraction of the time spent on p0->p1
        float yaw = 0.0f;
        bool climb_after = false;   // enter climbing when done (climb down from roof), otherwise back to locomotion
    };

    struct ProbeHit
    {
        bool hit = false;           // anything (except player itself) was hit
        bool building = false;      // hit object is a BuildingPart
        bool climbable = false;     // hit object is a climbable BuildingPart
        engine_game_object_t go = ENGINE_INVALID_GAME_OBJECT_ID;
        glm::vec3 position{ 0.0f };
        glm::vec3 normal{ 0.0f };
    };

    // Traversal available in the movement direction. Shown by the marker while approaching,
    // started automatically once the player reaches the obstacle.
    struct TraversalPlan
    {
        enum class Type
        {
            eNone,
            eClimb,
            eVault,
            eClimbDown
        };
        Type type = Type::eNone;
        float distance = 0.0f;              // how far the player still has to go before the traversal starts
        glm::vec3 marker{ 0.0f };           // where the indicator is shown
        glm::vec3 wall_normal{ 0.0f };
        glm::vec3 climb_position{ 0.0f };   // eClimb only
        TraversalMove move{};               // eVault and eClimbDown only
    };

public:
    Player(engine::IScene* my_scene, const PrefabResult& pr);

    void update(float dt);
    void on_confirmed_attack_hit(float hit_stop_seconds);
    bool receive_damage(std::uint32_t damage, const glm::vec3& source_position);

    bool equip_waepon(Weapon* sword);

    void add_coin(std::uint64_t amount);

private:
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
    bool attack_input_was_down_ = false;
    AttackPhase attack_phase_ = AttackPhase::eIdle;
    bool combat_layer_has_pose_ = false;
    float hit_stop_remaining_seconds_ = 0.0f;
    bool hit_stop_just_started_ = false;
    std::uint32_t hp_ = 100;
    bool dodge_input_was_down_ = false;
    float dodge_remaining_seconds_ = 0.0f;
    float dodge_invulnerability_remaining_seconds_ = 0.0f;
    float dodge_cooldown_remaining_seconds_ = 0.0f;
    float hit_stun_remaining_seconds_ = 0.0f;
    glm::vec3 dodge_direction_{ 0.0f, 0.0f, 1.0f };
    glm::vec3 knockback_velocity_{ 0.0f };

    AttackTrigger* attack_trigger_;

    // inventory
    Weapon* weapon_;
    std::uint64_t coins_ = 0;

    bool start_attack(engine_animation_controller_t* animation_controller, std::size_t combo_index);
    bool start_dodge(engine_animation_controller_t* animation_controller, const glm::vec2& move_input, const glm::quat& rotation);

    // traversal
    TraversalState traversal_state_ = TraversalState::eNone;
    TraversalMove traversal_move_{};
    glm::vec3 climb_wall_normal_{ 0.0f, 0.0f, 1.0f };
    float traversal_regrab_cooldown_ = 0.0f;
    engine_game_object_t traversal_marker_ = ENGINE_INVALID_GAME_OBJECT_ID;
    TraversalPlan::Type traversal_marker_type_ = TraversalPlan::Type::eNone;
    float traversal_marker_time_ = 0.0f;

    ProbeHit probe(const glm::vec3& from, const glm::vec3& to) const;
    TraversalPlan find_traversal(const glm::vec3& position, const glm::vec3& direction) const;
    TraversalPlan find_nearest_traversal(const glm::vec3& position) const;
    TraversalPlan plan_vault(const glm::vec3& position, const ProbeHit& low_hit) const;
    TraversalPlan plan_climb_down(const glm::vec3& position, const glm::vec3& direction) const;
    void execute_traversal(engine_animation_controller_t* animation_controller, const TraversalPlan& plan);
    bool try_start_mantle(engine_animation_controller_t* animation_controller, const glm::vec3& position);
    void start_climbing(engine_animation_controller_t* animation_controller, const glm::vec3& wall_normal, const glm::vec3& position);
    void start_traversal_move(engine_animation_controller_t* animation_controller, const TraversalMove& move, const char* animation_name);
    void update_traversal(engine_animation_controller_t* animation_controller, float delta_seconds, bool drop_pressed);
    void stop_traversal(engine_animation_controller_t* animation_controller, float regrab_cooldown);
    void create_traversal_marker();
    void update_traversal_marker(const TraversalPlan& plan, float delta_seconds);
    void set_kinematic(bool kinematic);
    void set_position_and_yaw(const glm::vec3& position, float yaw);
};
} //namespace project_c