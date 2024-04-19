#pragma once

#include "engine.h"
#include <span>
#include "math_helpers.h"

namespace engine
{
glm::quat compute_animation_rotation(std::span<const glm::quat> channel, std::span<const float> timestamps, float animation_time);
glm::vec3 compute_animation_translation(std::span<const glm::vec3> channel, std::span<const float> timestamps, float animation_time);
glm::vec3 compute_animation_scale(std::span<const glm::vec3> channel, std::span<const float> timestamps, float animation_time);
}   // namespace engine