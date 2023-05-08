#pragma once
#include "iscene.h"

namespace pong
{
class MainMenuScene : public engine::IScene
{
public:
    MainMenuScene(engine_application_t app_handle, engine_result_code_t& engine_error_code, engine::IScene* temp);
};
}