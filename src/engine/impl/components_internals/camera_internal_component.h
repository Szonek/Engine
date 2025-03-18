#pragma once
#include "../graphics.h"
#include "../math_helpers.h"

namespace engine
{

struct CameraGpuData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 position;
};

struct camera_internal_component_t
{
    CameraGpuData data; // kepe data here, so we can read it (i.e. in world-to-screen converter function)
    bool computed_this_frame = false;  // cace results to not recompute the same data each frame
    engine::UniformBuffer camera_ubo = engine::UniformBuffer(sizeof(CameraGpuData));
};
}