#pragma once
#include "engine.h"

#include "iscript.h"

namespace pong
{
class CameraScript : public engine::IScript
{
public:
    CameraScript(engine::IScene *my_scene);
};
}
