#pragma once
#include "engine.h"

#include "iscript.h"

#include <string>

namespace pong
{
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

    virtual void set_target_screenspace_position(float y);

protected:
    engine_game_object_t score_go_;
    std::size_t score_ = 0;
    std::string score_str_ = "";
    float target_y = 0.5f;
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
} // namespace pong