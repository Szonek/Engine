#include "editor_camera.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace
{
float wrap_angle(float angle)
{
    return std::remainder(angle, glm::two_pi<float>());
}

glm::vec3 forward_from(float yaw, float pitch)
{
    return { std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch) };
}
}  // namespace

engine::EditorCamera::EditorCamera(const glm::vec3& eye, const glm::vec3& pivot)
    : pivot_(pivot)
{
    const auto to_pivot = pivot - eye;
    distance_ = std::clamp(glm::length(to_pivot), MIN_DISTANCE, MAX_DISTANCE);
    const auto forward = glm::length(to_pivot) > 0.0f ? glm::normalize(to_pivot) : glm::vec3(0.0f, 0.0f, -1.0f);
    yaw_ = std::atan2(forward.x, -forward.z);
    pitch_ = std::clamp(std::asin(std::clamp(forward.y, -1.0f, 1.0f)), -MAX_PITCH, MAX_PITCH);
}

void engine::EditorCamera::update(const editor_camera_input_t& input, float dt, float viewport_height, float fov_y)
{
    const auto& delta = input.mouse_delta;
    flying_ = input.rmb && !input.alt;
    const bool orbiting = !flying_ && ((input.mmb && !input.shift && !input.ctrl) || (input.lmb && input.alt));
    const bool panning = !flying_ && input.mmb && input.shift;
    const bool drag_zooming = !flying_ && input.mmb && input.ctrl && !input.shift;
    // the user takes over: a running glide stops where it is
    if (flying_ || orbiting || panning || drag_zooming || input.wheel != 0.0f)
    {
        transition_time_ = TRANSITION_TIME;
    }
    update_transition(dt);

    if (flying_)
    {
        // looks around the eye: the pivot stays in front of it, at the same distance
        const auto eye = get_eye();
        yaw_ = wrap_angle(yaw_ + delta.x * LOOK_SENSITIVITY);
        pitch_ = std::clamp(pitch_ - delta.y * LOOK_SENSITIVITY, -MAX_PITCH, MAX_PITCH);
        if (input.wheel != 0.0f)
        {
            set_fly_speed(fly_speed_ * std::pow(FLY_WHEEL_FACTOR, input.wheel));
        }

        glm::vec3 move(0.0f);
        move += get_forward() * (static_cast<float>(input.move_forward) - static_cast<float>(input.move_back));
        move += get_right() * (static_cast<float>(input.move_right) - static_cast<float>(input.move_left));
        move.y += static_cast<float>(input.move_up) - static_cast<float>(input.move_down);
        if (glm::length(move) > 0.0f)
        {
            const float speed = fly_speed_ * (input.shift ? FLY_BOOST : 1.0f);
            move = glm::normalize(move) * speed * (dt / 1000.0f);
        }
        pivot_ = eye + move + get_forward() * distance_;
        return;
    }

    if (orbiting)
    {
        // turntable, "grab the scene": dragging right shows its left side, dragging up shows it from below
        yaw_ = wrap_angle(yaw_ + delta.x * ORBIT_SENSITIVITY);
        pitch_ = std::clamp(pitch_ - delta.y * ORBIT_SENSITIVITY, -MAX_PITCH, MAX_PITCH);
    }
    else if (panning)
    {
        // world units per pixel at the pivot's depth, so the point under the cursor follows it
        const float units_per_pixel = 2.0f * distance_ * std::tan(fov_y * 0.5f) / std::max(viewport_height, 1.0f);
        pivot_ += (-get_right() * delta.x + get_up() * delta.y) * units_per_pixel;
    }
    else if (drag_zooming)
    {
        distance_ = std::clamp(distance_ * std::exp(delta.y * DRAG_ZOOM_SENSITIVITY), MIN_DISTANCE, MAX_DISTANCE);
    }

    if (input.wheel != 0.0f)
    {
        distance_ = std::clamp(distance_ * std::pow(WHEEL_ZOOM_FACTOR, input.wheel), MIN_DISTANCE, MAX_DISTANCE);
    }
}

void engine::EditorCamera::frame(const glm::vec3& center, float radius, float fov_y)
{
    view_t to{ center, yaw_, pitch_, distance_ };
    to.distance = std::clamp(std::max(radius, MIN_DISTANCE) / std::sin(fov_y * 0.5f), MIN_DISTANCE, MAX_DISTANCE);
    start_transition(to);
}

void engine::EditorCamera::set_view(float yaw, float pitch)
{
    start_transition({ pivot_, wrap_angle(yaw), std::clamp(pitch, -MAX_PITCH, MAX_PITCH), distance_ });
}

glm::vec3 engine::EditorCamera::get_eye() const
{
    return pivot_ - get_forward() * distance_;
}

glm::vec3 engine::EditorCamera::get_forward() const
{
    return forward_from(yaw_, pitch_);
}

glm::vec3 engine::EditorCamera::get_right() const
{
    return { std::cos(yaw_), 0.0f, std::sin(yaw_) };
}

glm::vec3 engine::EditorCamera::get_up() const
{
    return glm::cross(get_right(), get_forward());
}

void engine::EditorCamera::set_fly_speed(float speed)
{
    fly_speed_ = std::clamp(speed, MIN_FLY_SPEED, MAX_FLY_SPEED);
}

void engine::EditorCamera::start_transition(const view_t& to)
{
    transition_from_ = { pivot_, yaw_, pitch_, distance_ };
    transition_to_ = to;
    transition_time_ = 0.0f;
}

void engine::EditorCamera::update_transition(float dt)
{
    if (transition_time_ >= TRANSITION_TIME)
    {
        return;
    }
    transition_time_ = std::min(transition_time_ + dt, TRANSITION_TIME);
    // ease out (cubic)
    const float t = 1.0f - std::pow(1.0f - transition_time_ / TRANSITION_TIME, 3.0f);
    const auto& from = transition_from_;
    const auto& to = transition_to_;
    pivot_ = glm::mix(from.pivot, to.pivot, t);
    yaw_ = wrap_angle(from.yaw + wrap_angle(to.yaw - from.yaw) * t);
    pitch_ = glm::mix(from.pitch, to.pitch, t);
    // zooming feels even in log space
    distance_ = std::exp(glm::mix(std::log(from.distance), std::log(to.distance), t));
}
