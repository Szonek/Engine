#pragma once
#include "engine.h"

#include "iscript.h"

#include <span>

namespace pong
{
class BallScript : public engine::IScript
{
public:
    BallScript(engine_application_t& app, engine_scene_t& scene);

    void reset_state();
    void update_linear_velocity(float dir_x, float dir_y);
    void update_linear_velocity(std::span<const float> dir);
    std::array<float, 2> get_direction_vector() const;

    void update(float dt) override;

private:
    void handle_input(float dt);

private:
    float ball_speed_x_ = 0.0f;
    float ball_speed_y_ = 0.0f;
    float timer_accu = 0.0f;
};
} // namespace pong