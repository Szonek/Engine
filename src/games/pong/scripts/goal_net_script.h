#pragma once
#include "engine.h"

#include "iscript.h"

namespace pong
{
class GoalNetScript : public engine::IScript
{
public:
    class BallScript* ball_script_ = nullptr;
    class PlayerPaddleScript* player_paddel_script_ = nullptr;

public:
    GoalNetScript(engine::IScene *my_scene, float init_pos_x, const char* name);

    void update(float dt) override;

    void on_collision(const collision_t& info) override;

protected:
    struct score_collision_fence
    {
        bool was_score = false;
        std::int32_t frame_counter = 0;
        constexpr static const std::int32_t COUNTER_LIMIT = 10;
    };

protected:
    std::uint32_t score_;
    score_collision_fence score_fence_;

};

class LeftGoalNetScript : public GoalNetScript
{
public:
    LeftGoalNetScript(engine::IScene *my_scene);
};

class RightGoalNetScript : public GoalNetScript
{
public:
    RightGoalNetScript(engine::IScene *my_scene);
};
} // namespace pong