#pragma once
#include "editor_snap.h"

#include <glm/glm.hpp>

namespace engine
{
enum class ModalTransformMode
{
    eGrab,
    eRotate,
    eScale,
};

enum class ModalTransformAxis
{
    eNone,
    eX,
    eY,
    eZ,
};

// The camera the mouse is read against: screen positions are pixels in the scene viewport, y down.
struct modal_transform_view_t
{
    glm::mat4 view{ 1.0f };
    glm::mat4 projection{ 1.0f };
    glm::vec2 viewport_size{ 1.0f };
};

// The ray under a screen position (viewport pixels, y down) in world space: its origin on the near plane and its
// direction (unit length).
void mouse_ray(const modal_transform_view_t& view, const glm::vec2& mouse, glm::vec3& origin, glm::vec3& direction);

// Blender's modal transform (G, R, S), GL-free: follows the mouse from where the key was pressed and gives the new
// world matrix; the editor confirms or cancels it.
//   grab:   the object moves under the cursor at its depth; along an axis, to the point of the axis under the cursor
//   rotate: by the angle the cursor turns around the object on the screen; about the view axis, or the chosen axis
//   scale:  by how much closer to / further from the object the cursor is; uniform, or along the chosen local axis
// Axis keys cycle like Blender's: first press global, second local, third none (scale: local, then none).
// Snapping (Ctrl): grab, rotate and scale in the editor's snap steps, like the gizmo.
class ModalTransform
{
public:
    ModalTransform(ModalTransformMode mode, const glm::mat4& world, const glm::vec2& mouse, const modal_transform_view_t& view,
        const editor_snap_steps_t& snap_steps = {});

    // starts the new mode from the current result and mouse position, like pressing another of G/R/S in Blender
    void set_mode(ModalTransformMode mode, const glm::vec2& mouse);
    // the motion since the start is read along the new axis
    void toggle_axis(ModalTransformAxis axis);

    // the world matrix for the mouse position
    glm::mat4 update(const glm::vec2& mouse, bool snap);

    ModalTransformMode get_mode() const { return mode_; }
    ModalTransformAxis get_axis() const { return axis_; }
    bool is_local() const { return local_; }
    // the constraint axis in world space (unit length), or zero without one
    glm::vec3 get_axis_direction() const;
    // the object's origin when the transform started, in viewport pixels
    glm::vec2 get_center_on_screen() const;
    glm::vec3 get_origin() const { return glm::vec3(start_world_[3]); }
    glm::mat4 get_result() const { return result_; }
    // a world position in viewport pixels
    glm::vec2 project(const glm::vec3& world) const;
    // what the last update did: the grab offset, the rotation in degrees (x) or the scale factor (x)
    glm::vec3 get_value() const { return value_; }

private:
    void restart(const glm::vec2& mouse);
    // grab along an axis starts measuring at the mouse position
    void restart_axis(const glm::vec2& mouse);
    // how far along the constraint axis (from the origin) the cursor points, or false when the ray is parallel to it
    bool axis_parameter(const glm::vec2& mouse, float& out) const;
    float screen_angle(const glm::vec2& mouse) const;

private:
    ModalTransformMode mode_;
    ModalTransformAxis axis_ = ModalTransformAxis::eNone;
    bool local_ = false;
    modal_transform_view_t view_;
    editor_snap_steps_t snap_steps_;
    glm::mat4 view_projection_{ 1.0f };
    glm::mat4 inverse_view_projection_{ 1.0f };

    glm::mat4 start_world_;
    glm::vec2 start_mouse_;
    glm::mat4 result_;
    glm::vec3 value_{ 0.0f };

    // grab along an axis: where the cursor started on it; unknown while the axis points at the camera
    float start_axis_parameter_ = 0.0f;
    bool has_start_axis_parameter_ = false;
    float last_axis_parameter_ = 0.0f;
    // rotate: the angle is accumulated, so it can go past half a turn
    float last_screen_angle_ = 0.0f;
    float angle_ = 0.0f;
};
}  // namespace engine
