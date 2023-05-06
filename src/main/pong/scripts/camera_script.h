#pragma once
#include "engine.h"

#include "iscript.h"

namespace pong
{
class CameraScript : public engine::IScript
{
public:
    CameraScript(engine_application_t& app, engine_scene_t& scene);
};
}
