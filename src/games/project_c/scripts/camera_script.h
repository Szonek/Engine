#pragma once
#include "base_script.h"

namespace project_c
{
class CameraScript : public BaseNode
{
public:
    CameraScript(engine::IScene* my_scene);
    void late_update(float dt) override;
    void shake(float intensity, float duration_seconds);

private:
    float shake_remaining_seconds_ = 0.0f;
    float shake_duration_seconds_ = 0.0f;
    float shake_elapsed_seconds_ = 0.0f;
    float shake_intensity_ = 0.0f;
};

}
