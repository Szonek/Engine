#pragma once

#include "iscript.h"

namespace engine
{
struct IScene;
struct PointerEventData;
}

namespace pong
{

class PlayerTouchAreaScript : public engine::IScript
{
public:
    PlayerTouchAreaScript(engine::IScene *my_scene, float start_pos_x, float end_pos_x, const char* name);

    void on_pointer_down(const engine::PointerEventData* ped) override;

public:
    class PlayerPaddleScript* player_script_ = nullptr;
};

class RightPlayerTouchAreaScript : public PlayerTouchAreaScript
{
public:
    RightPlayerTouchAreaScript(engine::IScene *my_scene);
};

class LeftPlayerTouchAreaScript : public PlayerTouchAreaScript
{
public:
    LeftPlayerTouchAreaScript(engine::IScene *my_scene);
};

class PlayerSuperPower_TouchAreaScript : public engine::IScript
{
public:
    PlayerSuperPower_TouchAreaScript(engine::IScene* my_scene, float start_pos_x, float end_pos_x, const char* name);


    void update(float dt) override;
    void on_pointer_click(const engine::PointerEventData* ped) override;

public:
    class PlayerPaddleScript* player_script_ = nullptr;
};

class LeftPlayerSuperPower_0_TouchAreaScript : public PlayerSuperPower_TouchAreaScript
{
public:
    LeftPlayerSuperPower_0_TouchAreaScript(engine::IScene* my_scene);
};

class RightPlayerSuperPower_0_TouchAreaScript : public PlayerSuperPower_TouchAreaScript
{
public:
    RightPlayerSuperPower_0_TouchAreaScript(engine::IScene *my_scene);
};
} // namespace pong