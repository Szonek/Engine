#pragma
#include "base_script.h"

#include <array>
#include <vector>

namespace project_c
{
class Enemy;

class EnemyWeapon : public BaseNode
{
public:
    EnemyWeapon(engine::IScene* my_scene, engine_game_object_t go, Enemy* enemy);

    void begin_swing();
    void set_attack_window_active(bool active);
    void on_collision(const collision_t& info) override;

private:
    Enemy* enemy_ = nullptr;
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

    Enemy(engine::IScene* my_scene, const PrefabResult& pr, float offset_x, float offset_z);

    void update(float dt);
    void receive_attack_hit(std::uint32_t damage, float hit_stop_seconds);
    void set_attack_window_active(bool active);
    void on_weapon_hit_player(engine_game_object_t player_go);

private:
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
    AttackStateData attack_data_;
    std::vector<IScript*> debug_scripts_;
    std::vector<HitFlashMaterial> hit_flash_materials_;
    EnemyWeapon* weapon_ = nullptr;
    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;  // cache the result, because  it's performance expensive to find player go with each frame
    float hit_flash_remaining_seconds_ = 0.0f;
    float hit_stop_remaining_seconds_ = 0.0f;
    float attack_cooldown_remaining_seconds_ = 0.0f;
    bool hit_stop_just_started_ = false;
};
} //namespace project_c