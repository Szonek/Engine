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
            return attack_with_right ? "1H_Melee_Attack_Slice_Diagonal" : "1H_Melee_Attack_Slice_Horizontal";
        }
    };
    struct DyingStateData {};
    struct MoveStateData {};

public:
    std::uint32_t hp = 20;
    std::uint32_t max_hp = 20;

    Enemy(engine::IScene* my_scene, const PrefabResult& pr, const class NavMesh* nav_mesh, float offset_x, float offset_z);
    virtual ~Enemy();

    void update(float dt);

private:
    const class NavMesh* nav_mesh_;
    bool triggered_ = false;
    bool attack_right_ = false;
    States state_;
    AttackStateData attack_data_;
    std::vector<IScript*> debug_scripts_;
    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;  // cache the result, because  it's performance expensive to find player go with each frame
};
} //namespace project_c