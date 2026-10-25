#pragma once
#include <cmath>

namespace engine
{
// The steps the editor's transform tools snap to while Ctrl is held: the modal transform (G, R, S) and the gizmo
// move, rotate and scale by the same steps, counted from where the transform started.
struct editor_snap_steps_t
{
    float translate = 0.5f;  // units
    float rotate = 5.0f;  // degrees
    float scale = 0.1f;  // scale factor
};

// the multiple of `step` closest to `value`
inline float snap_to(float value, float step)
{
    return std::round(value / step) * step;
}
}  // namespace engine
