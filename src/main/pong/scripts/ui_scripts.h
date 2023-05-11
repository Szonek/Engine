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


// MAIN MENU
class MainMenuStartPveScene : public engine::IScript
{
public:
    MainMenuStartPveScene(engine::IScene *my_scene);

    void on_pointer_click(const engine::PointerEventData* ped) override;
};

// MAIN MENU
class MainMenuStartPvpScene : public engine::IScript
{
public:
    MainMenuStartPvpScene(engine::IScene *my_scene);

    void on_pointer_click(const engine::PointerEventData* ped) override;
};

} // namespace pong