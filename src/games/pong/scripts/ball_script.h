#pragma once
#include "engine.h"

#include "iscript.h"

#include <span>

namespace pong
{
class BallScript : public engine::IScript
{
public:
    BallScript(engine::IScene *my_scene);

    void update_linear_velocity(float dir_x, float dir_y);
    void update_linear_velocity(std::span<const float> dir);
    void update_speed(float speed_x, float speed_y);
    void update_diffuse_color(std::array<float, 4> color);
    std::array<float, 2> get_direction_vector() const;
    std::array<float, 2> get_current_position() const;
    std::array<float, 2> get_speed() const;
    void update(float dt) override;

private:
    void reset_state();

private:
    float ball_speed_x_ = 0.0f;
    float ball_speed_y_ = 0.0f;
    float timer_accu_ = 0.0f;
};
} // namespace pong