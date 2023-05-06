#pragma once
#include "engine.h"

#include "../iscript.h"

class WallScript : public IScript
{
public:
    WallScript(engine_application_t& app, engine_scene_t& scene, float init_pos_y, const char* name);
};

class WallTopScript : public WallScript
{
public:
    WallTopScript(engine_application_t& app, engine_scene_t& scene);
};

class BottomTopScript : public WallScript
{
public:
    BottomTopScript(engine_application_t& app, engine_scene_t& scene);
};