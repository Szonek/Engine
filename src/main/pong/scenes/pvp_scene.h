#pragma once
#include "iscene.h"

namespace pong
{
class PvpScene : public engine::IScene
{
public:
    PvpScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code);

    static constexpr const char* get_name() { return "pvp_scene"; }
};
}