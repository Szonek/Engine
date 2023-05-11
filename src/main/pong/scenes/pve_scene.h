#pragma once
#include "iscene.h"

namespace pong
{
class PveScene : public engine::IScene
{
public:
    PveScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code);

public:
    static constexpr const char* get_name() { return "pve_scene"; }
};
}