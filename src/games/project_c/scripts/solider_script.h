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

class Sword : public BaseNode
{
public:
    Sword(engine::IScene* my_scene, engine_game_object_t go);
    ~Sword();

    void attach_to_game_object(engine_game_object_t parent, std::optional<glm::vec3> position, std::optional<glm::quat> rotation);
    void deattach_from_parent(glm::vec3 position);

    void on_collision(const collision_t& info) override;

private:
    engine_ui_document_t ui_document_ = nullptr;
    engine_ui_data_handle_t ui_data_handle_ = nullptr;
    struct ui_data
    {
        std::uint32_t name = 0;
    };
    ui_data ui_data_;
};

class Dagger : public BaseNode
{
public:
    struct Config
    {
        glm::vec3 start_position;
        glm::quat direction;
        std::uint32_t ricochet_count = 1;
        engine_game_object_t ignore_go = ENGINE_INVALID_GAME_OBJECT_ID;
        bool destroy_on_next_frame = false;
    };
    Dagger(engine::IScene* my_scene, engine_game_object_t go, const Config& config);

    void update(float dt) override;
    void on_collision(const collision_t& info) override;
private:
    Config config_;
};

class Solider : public BaseNode
{
private:
    enum States : std::uint32_t
    {
        IDLE    = 0x0000,
        ATTACK  = 0x0001,
        MOVE    = 0x0002,
        DODGE   = 0x0004,
        SKILL_1 = 0x0008,

    };

    struct GlobalStateData
    {
        engine_ray_hit_info_t last_mouse_hit = {};
    };

    struct DodgeStateData
    {
        std::chrono::milliseconds dodge_timer_cooldown = std::chrono::milliseconds(0);
        std::chrono::milliseconds dodge_timer_animation = std::chrono::milliseconds(0);

        void update(float dt)
        {
            if (animation_playing_)
            {
                dodge_timer_animation += std::chrono::milliseconds(static_cast<std::int64_t>(dt));
            }
            if (cooldown_playing_)
            {
                dodge_timer_cooldown += std::chrono::milliseconds(static_cast<std::int64_t>(dt));
            }

            if (dodge_timer_animation >= std::chrono::milliseconds(150))
            {
                dodge_timer_animation = std::chrono::milliseconds(0);
                animation_playing_ = false;
            }
            if (dodge_timer_cooldown >= std::chrono::milliseconds(3000))
            {
                dodge_timer_cooldown = std::chrono::milliseconds(0);
                cooldown_playing_ = false;
            }
        }

        inline bool animation_is_playing() const
        {
            return animation_playing_;
        }

        inline void activate()
        {
            animation_playing_ = true;
            cooldown_playing_ = true;
        }

        inline bool can_dodge() const
        {
            return !cooldown_playing_;
        }
    private:
        bool animation_playing_ = false;
        bool cooldown_playing_ = false;
    };

    struct MoveStateData
    {
        bool animation_started = false;
        inline const char* get_animation_name() const
        {
            return "walk";
        }
    };

    struct AttackStateData
    {
        bool animation_started = false;
        inline const char* get_animation_name() const
        {
            return "attack-melee-right";
        }
    };

    struct Skill_1_StateData
    {
        bool animation_started = false;
        inline const char* get_animation_name() const
        {
            return "attack-melee-left";
        }
    };

public:
    Solider(engine::IScene* my_scene, const PrefabResult& pr);

    void update(float dt);

private:
    std::uint32_t state_;
    MoveStateData move_data_;
    AttackStateData attack_data_;
    Skill_1_StateData skill_1_data_;
    GlobalStateData global_data_;
    DodgeStateData dodge_data_;

    AttackTrigger* attack_trigger_;

    // child objects
    engine_game_object_t right_arm_go_ = ENGINE_INVALID_GAME_OBJECT_ID;

    // inventory
    Sword* weapon_;
};
} //namespace project_c