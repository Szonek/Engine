#pragma once
#include "engine.h"

#include "iscript.h"

#include <string>
#include <deque>

namespace pong
{
enum class SuperPowerState
{
    eNone,
    eTrigger = 1,
    eActive
};
enum class SuperPowerType
{
    eNone,
    eBallSuperSpeed = 1
};

class PlayerPaddleScript : public engine::IScript
{
public:
    class BallScript* ball_script_ = nullptr;

public:
    PlayerPaddleScript(engine::IScene *my_scene, float init_pos_x, float score_init_pos_x, const char* name);

    void on_collision(const collision_t& info) override;
    void update(float dt) override;

    virtual void set_score(std::size_t new_score);
    virtual std::size_t get_score() const;

    virtual void set_target_worldspace_position(float y);
    void trigger_super_power();
    virtual SuperPowerType get_super_power_type() const { return super_power_type_; }


protected:
    engine_game_object_t score_go_;
    std::size_t score_ = 0;
    std::string score_str_ = "";
    float target_y = 0.5f;

    SuperPowerType super_power_type_ = SuperPowerType::eNone;
    SuperPowerState super_power_state_ = SuperPowerState::eNone;
    float timer_superpower_cd_ = 0.0f;
    float timer_superpower_active_cd_ = 0.0f;
};

class RightPlayerPaddleScript : public PlayerPaddleScript
{
public:
    RightPlayerPaddleScript(engine::IScene *my_scene);
};


class LeftPlayerPaddleScript : public PlayerPaddleScript
{
public:
    LeftPlayerPaddleScript(engine::IScene *my_scene);
};

class BotPlayerPaddleScript : public PlayerPaddleScript
{
public:
    enum class Difficulty
    {
        eEasy,
        eMedium,
        eHard,
        eGodLike
    };

public:
    BotPlayerPaddleScript(engine::IScene *my_scene);

    void set_difficulty(Difficulty difficulty);
    void update(float dt) override;

private:
    Difficulty difficulty_;
    std::deque<float> ball_history_;
};

} // namespace pong