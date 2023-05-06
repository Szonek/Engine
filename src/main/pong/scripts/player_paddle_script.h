#pragma once
#include "engine.h"

#include "iscript.h"

#include <string>

class PlayerPaddleScript : public IScript
{
public:
    class BallScript* ball_script_ = nullptr;

public:
    PlayerPaddleScript(engine_application_t& app, engine_scene_t& scene, float init_pos_x, float score_init_pos_x, const char* name);

    void on_collision(const collision_t& info) override;
    void update(float dt) override;

    virtual void set_score(std::size_t new_score);
    virtual std::size_t get_score() const;

protected:
    void handle_input(float dt);
    virtual bool is_finger_in_controller_area_impl(const engine_finger_info_t& f) = 0;

protected:
    engine_game_object_t score_go_;
    std::size_t score_ = 0;
    std::string score_str_ = "";
};

class RightPlayerPaddleScript : public PlayerPaddleScript
{
public:
    RightPlayerPaddleScript(engine_application_t& app, engine_scene_t& scene);

protected:
    bool is_finger_in_controller_area_impl(const engine_finger_info_t& f) override;
};


class LeftPlayerPaddleScript : public PlayerPaddleScript
{
public:
    LeftPlayerPaddleScript(engine_application_t& app, engine_scene_t& scene);

protected:
    bool is_finger_in_controller_area_impl(const engine_finger_info_t& f) override;
};
