#pragma once
#include "../render/render_scene_data.h"
#include "../math_helpers.h"

namespace engine
{
struct camera_internal_component_t
{
    CameraGpuData data; // kepe data here, so we can read it (i.e. in world-to-screen converter function)
    bool computed_this_frame = false;  // cace results to not recompute the same data each frame
};
}