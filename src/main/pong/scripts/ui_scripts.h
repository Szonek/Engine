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
    PlayerTouchAreaScript(engine_application_t& app, engine_scene_t& scene, float start_pos_x, float end_pos_x, const char* name);

    void on_pointer_down(const engine::PointerEventData* ped) override;

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


// MAIN MENU
class MainMenuStartPveScene : public engine::IScript
{
public:
    engine::IScene* my_scene_ = nullptr;
    engine::IScene* pve_scene_ = nullptr;
public:
    MainMenuStartPveScene(engine_application_t& app, engine_scene_t& scene);

    void on_pointer_click(const engine::PointerEventData* ped) override;
};


} // namespace pong