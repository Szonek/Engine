#include "editor_modal_transform.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

engine::ModalTransform::ModalTransform(ModalTransformMode mode, const glm::mat4& world, const glm::vec2& mouse, const modal_transform_view_t& view,
    const editor_snap_steps_t& snap_steps)
    : mode_(mode)
    , view_(view)
    , snap_steps_(snap_steps)
    , view_projection_(view.projection * view.view)
    , inverse_view_projection_(glm::inverse(view_projection_))
    , start_world_(world)
    , start_mouse_(mouse)
    , result_(world)
{
    restart(mouse);
}

void engine::ModalTransform::set_mode(ModalTransformMode mode, const glm::vec2& mouse)
{
    mode_ = mode;
    start_world_ = result_;
    start_mouse_ = mouse;
    if (mode_ == ModalTransformMode::eScale && axis_ != ModalTransformAxis::eNone)
    {
        local_ = true;  // scale has no global axes
    }
    restart(mouse);
}

void engine::ModalTransform::toggle_axis(ModalTransformAxis axis)
{
    if (axis != axis_)
    {
        axis_ = axis;
        local_ = mode_ == ModalTransformMode::eScale;
    }
    else if (!local_)
    {
        local_ = true;
    }
    else
    {
        axis_ = ModalTransformAxis::eNone;
        local_ = false;
    }
    restart_axis(start_mouse_);
}

glm::mat4 engine::ModalTransform::update(const glm::vec2& mouse, bool snap)
{
    const auto origin = get_origin();
    const auto axis = get_axis_direction();
    switch (mode_)
    {
    case ModalTransformMode::eGrab:
    {
        glm::vec3 offset(0.0f);
        if (axis_ == ModalTransformAxis::eNone)
        {
            // the origin keeps its depth and follows the cursor's motion on the screen
            const auto clip = view_projection_ * glm::vec4(origin, 1.0f);
            const auto delta = mouse - start_mouse_;
            const auto ndc_delta = glm::vec2(2.0f * delta.x / view_.viewport_size.x, -2.0f * delta.y / view_.viewport_size.y);
            const auto moved = inverse_view_projection_ * (clip + glm::vec4(ndc_delta * clip.w, 0.0f, 0.0f));
            offset = glm::vec3(moved) / moved.w - origin;
            if (snap)
            {
                offset = { snap_to(offset.x, snap_steps_.translate), snap_to(offset.y, snap_steps_.translate), snap_to(offset.z, snap_steps_.translate) };
            }
        }
        else
        {
            float parameter = 0.0f;
            if (axis_parameter(mouse, parameter))
            {
                if (!has_start_axis_parameter_)
                {
                    // the axis pointed at the camera at the start: the motion is measured from here
                    start_axis_parameter_ = parameter;
                    has_start_axis_parameter_ = true;
                }
                last_axis_parameter_ = parameter;
            }
            float distance = last_axis_parameter_ - start_axis_parameter_;
            if (snap)
            {
                distance = snap_to(distance, snap_steps_.translate);
            }
            offset = axis * distance;
        }
        value_ = offset;
        result_ = glm::translate(glm::mat4(1.0f), offset) * start_world_;
        break;
    }
    case ModalTransformMode::eRotate:
    {
        const float screen = screen_angle(mouse);
        angle_ += std::remainder(screen - last_screen_angle_, glm::two_pi<float>());
        last_screen_angle_ = screen;
        float angle = angle_;
        if (snap)
        {
            angle = glm::radians(snap_to(glm::degrees(angle), snap_steps_.rotate));
        }
        // counterclockwise on the screen turns about the axis toward the viewer
        const auto toward_viewer = glm::normalize(glm::vec3(glm::inverse(view_.view)[2]));
        auto rotation_axis = toward_viewer;
        if (axis_ != ModalTransformAxis::eNone)
        {
            rotation_axis = glm::dot(axis, toward_viewer) >= 0.0f ? axis : -axis;
        }
        value_ = glm::vec3(glm::degrees(angle), 0.0f, 0.0f);
        const auto rotation = glm::mat4_cast(glm::angleAxis(angle, rotation_axis));
        result_ = glm::translate(glm::mat4(1.0f), origin) * rotation * glm::translate(glm::mat4(1.0f), -origin) * start_world_;
        break;
    }
    case ModalTransformMode::eScale:
    {
        const auto center = get_center_on_screen();
        const float start_distance = std::max(glm::length(start_mouse_ - center), 1.0f);
        float factor = glm::length(mouse - center) / start_distance;
        if (snap)
        {
            factor = snap_to(factor, snap_steps_.scale);
        }
        glm::vec3 scale(factor);
        if (axis_ != ModalTransformAxis::eNone)
        {
            scale = glm::vec3(1.0f);
            scale[static_cast<int>(axis_) - 1] = factor;
        }
        value_ = glm::vec3(factor, 0.0f, 0.0f);
        result_ = start_world_ * glm::scale(glm::mat4(1.0f), scale);
        break;
    }
    }
    return result_;
}

glm::vec3 engine::ModalTransform::get_axis_direction() const
{
    if (axis_ == ModalTransformAxis::eNone)
    {
        return glm::vec3(0.0f);
    }
    const int index = static_cast<int>(axis_) - 1;
    if (local_)
    {
        const auto column = glm::vec3(start_world_[index]);
        if (glm::length(column) > 0.0f)
        {
            return glm::normalize(column);
        }
    }
    glm::vec3 direction(0.0f);
    direction[index] = 1.0f;
    return direction;
}

glm::vec2 engine::ModalTransform::get_center_on_screen() const
{
    return project(get_origin());
}

void engine::ModalTransform::restart_axis(const glm::vec2& mouse)
{
    has_start_axis_parameter_ = axis_parameter(mouse, start_axis_parameter_);
    if (!has_start_axis_parameter_)
    {
        start_axis_parameter_ = 0.0f;
    }
    last_axis_parameter_ = start_axis_parameter_;
}

void engine::ModalTransform::restart(const glm::vec2& mouse)
{
    restart_axis(mouse);
    last_screen_angle_ = screen_angle(mouse);
    angle_ = 0.0f;
    value_ = mode_ == ModalTransformMode::eGrab ? glm::vec3(0.0f) : glm::vec3(mode_ == ModalTransformMode::eScale ? 1.0f : 0.0f, 0.0f, 0.0f);
}

glm::vec2 engine::ModalTransform::project(const glm::vec3& world) const
{
    const auto clip = view_projection_ * glm::vec4(world, 1.0f);
    const auto ndc = glm::vec2(clip) / clip.w;
    return { (ndc.x + 1.0f) * 0.5f * view_.viewport_size.x, (1.0f - ndc.y) * 0.5f * view_.viewport_size.y };
}

void engine::ModalTransform::mouse_ray(const glm::vec2& mouse, glm::vec3& origin, glm::vec3& direction) const
{
    const auto ndc = glm::vec2(2.0f * mouse.x / view_.viewport_size.x - 1.0f, 1.0f - 2.0f * mouse.y / view_.viewport_size.y);
    const auto near_point = inverse_view_projection_ * glm::vec4(ndc, -1.0f, 1.0f);
    const auto far_point = inverse_view_projection_ * glm::vec4(ndc, 1.0f, 1.0f);
    origin = glm::vec3(near_point) / near_point.w;
    direction = glm::normalize(glm::vec3(far_point) / far_point.w - origin);
}

bool engine::ModalTransform::axis_parameter(const glm::vec2& mouse, float& out) const
{
    if (axis_ == ModalTransformAxis::eNone)
    {
        return false;
    }
    // the point of the axis line closest to the mouse ray
    glm::vec3 ray_origin;
    glm::vec3 ray_direction;
    mouse_ray(mouse, ray_origin, ray_direction);
    const auto axis = get_axis_direction();
    const auto w0 = get_origin() - ray_origin;
    const float b = glm::dot(axis, ray_direction);
    const float d = glm::dot(axis, w0);
    const float e = glm::dot(ray_direction, w0);
    const float denominator = 1.0f - b * b;  // both directions have unit length
    if (denominator < 1e-4f)
    {
        return false;  // the axis points at the camera
    }
    out = (b * e - d) / denominator;
    return true;
}

float engine::ModalTransform::screen_angle(const glm::vec2& mouse) const
{
    const auto from_center = mouse - get_center_on_screen();
    return std::atan2(-from_center.y, from_center.x);
}
