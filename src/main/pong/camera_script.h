#pragma once
#include "engine.h"

#include "../iscript.h"

class CameraScript : public IScript
{
public:
    CameraScript(engine_application_t& app, engine_scene_t& scene);
};