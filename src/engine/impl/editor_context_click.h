#pragma once
#include "editor_camera.h"

#include <glm/glm.hpp>

namespace engine
{
// Tells a right-click in the scene view from flying the editor camera, like Unreal's and Unity's viewports: the right
// button held flies the camera, so only a press and release with the mouse moved less than a few pixels, no fly key
// or wheel used and no other button pressed meanwhile is a click (the one that opens the context menu). GL-free.
class ContextClick
{
public:
    // ImGui's default drag threshold
    static constexpr float MAX_TRAVEL = 6.0f;

    // Every frame, with what the editor camera reads; `over_scene`: the mouse is over the scene view and nothing else
    // owns it (a gizmo, a modal transform). True in the frame the button is released after a click.
    bool update(const editor_camera_input_t& input, bool over_scene)
    {
        if (!input.rmb)
        {
            const bool click = held_ && valid_;
            held_ = false;
            return click;
        }
        if (!held_)
        {
            // Alt + right button is left to the camera (and Alt + left orbits)
            held_ = true;
            valid_ = over_scene && !input.alt;
            travel_ = 0.0f;
        }
        travel_ += glm::length(input.mouse_delta);
        const bool flew = input.move_forward || input.move_back || input.move_left || input.move_right || input.move_down || input.move_up
            || input.wheel != 0.0f;
        if (flew || input.lmb || input.mmb || travel_ > MAX_TRAVEL)
        {
            valid_ = false;
        }
        return false;
    }

private:
    bool held_ = false;
    bool valid_ = false;
    // pixels the mouse moved while the button is held, along its path
    float travel_ = 0.0f;
};
}  // namespace engine
