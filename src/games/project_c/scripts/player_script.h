#pragma
#include "base_script.h"

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
    void drop_on_ground(glm::vec3 position);

    void on_collision(const collision_t& info) override;

    void update(float dt) override;
};

class Player : public BaseNode
{
private:
    enum States : std::uint32_t
    {
        IDLE   = 0x0000,
        ATTACK = 0x0002,

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
        static constexpr std::size_t combo_length = 3;
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
    Player(engine::IScene* my_scene, const PrefabResult& pr);

    void update(float dt);
    void on_confirmed_attack_hit(float hit_stop_seconds);

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
    float attack_duration_ = 0.0f;
    float attack_buffer_remaining_ = 0.0f;
    bool attack_buffered_ = false;
    bool attack_input_was_down_ = false;
    AttackPhase attack_phase_ = AttackPhase::eIdle;
    bool combat_layer_has_pose_ = false;
    float hit_stop_remaining_seconds_ = 0.0f;
    bool hit_stop_just_started_ = false;

    AttackTrigger* attack_trigger_;

    // inventory
    Weapon* weapon_;
    std::uint64_t coins_ = 0;

    bool start_attack(engine_animation_controller_t* animation_controller, std::size_t combo_index);
};
} //namespace project_c