#pragma once
#include "iscene.h"

namespace pong
{
class MainScene : public engine::IScene
{
public:
    MainScene(engine_application_t app_handle, engine_result_code_t& engine_error_code);
};
}