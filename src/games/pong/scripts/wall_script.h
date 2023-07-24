#pragma once
#include "engine.h"

#include "iscript.h"

namespace pong
{

class WallScript : public engine::IScript
{
public:
    WallScript(engine::IScene *my_scene, float init_pos_y, const char* name);
};

class WallTopScript : public WallScript
{
public:
    WallTopScript(engine::IScene *my_scene);
};

class BottomTopScript : public WallScript
{
public:
    BottomTopScript(engine::IScene *my_scene);
};

} // namespace pong