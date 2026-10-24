#pragma once
#include "base_script.h"
#include "game_components.h"

#include <array>
#include <cstdint>
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
    // the scale of the enemy's model in the level
    static constexpr float MODEL_SCALE = 0.35f;

    // set to the config's max_hp when the enemy starts
    std::uint32_t hp = 0;

    Enemy(engine::IScene* my_scene, engine_game_object_t go);
    // removes its animation events and its blade (also when its script component is removed and its game object stays)
    ~Enemy();

    // its designer-set values (the enemy_config component), read on every use
    EnemyConfig config() const;
    std::uint32_t get_max_hp() const { return config().max_hp; }

    void on_start() override;

    void update(float dt);
    void receive_attack_hit(std::uint32_t damage, float hit_stop_seconds);
    void set_attack_window_active(bool active);
    void on_weapon_hit_player(engine_game_object_t player_go);

private:
    // the player's game object, or none (a scene without a player, or the player is gone)
    void find_player();

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
    // the blade by game object, its script looked up on use: a script can go while its game object stays (its script
    // component removed in the editor), so no pointer to another script is kept
    EnemyWeapon* weapon() const;
    engine_game_object_t weapon_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    // the animation events calling it (removed with it), by animation
    std::vector<std::pair<const char*, engine_animation_event_id_t>> animation_events_;
    engine_game_object_t player_go_ = ENGINE_INVALID_GAME_OBJECT_ID;  // found in on_start, again when it is gone
    float hit_flash_remaining_seconds_ = 0.0f;
    float hit_stop_remaining_seconds_ = 0.0f;
    float attack_cooldown_remaining_seconds_ = 0.0f;
    bool hit_stop_just_started_ = false;
};
} //namespace project_c