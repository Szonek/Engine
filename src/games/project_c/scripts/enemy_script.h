#pragma
#include "base_script.h"

namespace project_c
{

class EnemyHealthBar : public BaseNode
{
public:
    EnemyHealthBar(engine::IScene* my_scene, const class Enemy* enemy);
    virtual ~EnemyHealthBar();

    void update(float dt);
private:
    const class Enemy* enemy_;
    const std::int32_t max_hp_;
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

    Enemy(engine::IScene* my_scene, const PrefabResult& pr, const class NavMesh* nav_mesh, float offset_x, float offset_z);
    virtual ~Enemy();

    void update(float dt);

private:
    EnemyHealthBar* health_bar_script_;
    const class NavMesh* nav_mesh_;
    bool triggered_ = false;
    bool attack_right_ = false;
    States state_;
    AttackStateData attack_data_;
    std::vector<IScript*> debug_scripts_;


};
} //namespace project_c