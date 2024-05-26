#pragma once
#include "base_script.h"

namespace project_c
{
class CameraScript : public BaseNode
{
public:
    CameraScript(engine::IScene* my_scene);
    void update(float dt) override;
};

}
