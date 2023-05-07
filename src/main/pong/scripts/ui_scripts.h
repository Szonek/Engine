#pragma once

#include "iscript.h"

namespace engine
{
struct PointerEventData;
}

namespace pong
{

class PlayerTouchAreaScript : public engine::IScript
{
public:
    PlayerTouchAreaScript(engine_application_t& app, engine_scene_t& scene, float start_pos_x, float end_pos_x, const char* name);

    void on_pointer_click(const engine::PointerEventData* ped) override;

public:
    class PlayerPaddleScript* player_script_ = nullptr;
};

class RightPlayerTouchAreaScript : public PlayerTouchAreaScript
{
public:
    RightPlayerTouchAreaScript(engine_application_t& app, engine_scene_t& scene);
};

class LeftPlayerTouchAreaScript : public PlayerTouchAreaScript
{
public:
    LeftPlayerTouchAreaScript(engine_application_t& app, engine_scene_t& scene);
};

} // namespace pong