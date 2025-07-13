#pragma
#include "base_script.h"

#include <chrono>

#include <glm/glm.hpp>

namespace project_c
{
class AttackTrigger : public BaseNode
{
public:
    AttackTrigger(engine::IScene* my_scene, engine_game_object_t go);

    void activate();
    void on_collision(const collision_t& info) override;
    void update(float dt) override;

private:
    bool is_active_ = false;
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
        IDLE           = 0x0000,
        TRIGGER_ATTACK = 0x0001,
        ATTACK         = 0x0002,
        MOVE           = 0x0004,
        PLACEHOLDER    = 0x0010,

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

        inline const char* get_animation_name(Direction dir) const
        {
            return "Running_A";
/*            switch (dir)
            {
            case Direction::eForward:
                return "Running_A";
            case Direction::eBackward:
                return "Running_A";
            case Direction::eLeft:
                return "Running_Strafe_Left";
            case Direction::eRight:
                return "Running_Strafe_Right";
            default:
                assert(!"Unknown move direction for player!");
            }
            return ""*/;
        }
    };

    struct AttackStateData
    {
        inline const char* get_animation_name() const
        {
            //return "1H_Melee_Attack_Chop";
            //return "1H_Melee_Attack_Slice_Horizontal";
            return "1H_Melee_Attack_Slice_Diagonal";
        }
    };

public:
    Player(engine::IScene* my_scene, const PrefabResult& pr);

    void update(float dt);

    bool equip_waepon(Weapon* sword);

    void add_coin(std::uint64_t amount);

private:
    inline static const std::size_t LOCOMOTION_LAYER_ID = 0;
    inline static const std::size_t COMBAT_LAYER_ID = 123;

    std::uint32_t state_;
    MoveStateData move_data_;
    AttackStateData attack_data_;
    GlobalStateData global_data_;

    AttackTrigger* attack_trigger_;

    // inventory
    Weapon* weapon_;
    std::uint64_t coins_ = 0;
};
} //namespace project_c