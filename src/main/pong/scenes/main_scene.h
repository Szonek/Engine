#pragma once
#include "iscene.h"

namespace pong
{
class MainScene : public engine::IScene
{
public:
    static constexpr const char* K_NAME = "pve_scene";
public:
    MainScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code);
};
}