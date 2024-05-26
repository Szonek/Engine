#pragma
#include "base_script.h"

namespace project_c
{

class Enemy : public BaseNode
{
private:
    enum class States
    {
        DECISION_MAKE = 0,
        IDLE = 1,
        ATTACK,
        DIE,
        MOVE
    };

    struct IdleStateData {};
    struct AttackStateData
    {
        bool attack_with_right = false;
        inline const char* get_animation_name() const
        {
            return attack_with_right ? "attack-melee-right" : "attack-melee-left";
        }
    };
    struct DyingStateData {};
    struct MoveStateData {};

public:
    std::int32_t hp = 20;
    Enemy(engine::IScene* my_scene, engine_game_object_t go, float offset_x, float offset_z);
    virtual ~Enemy();

    void update(float dt);

private:
    bool triggered_ = false;
    bool attack_right_ = false;
    States state_;
    AttackStateData attack_data_;

};
} //namespace project_c