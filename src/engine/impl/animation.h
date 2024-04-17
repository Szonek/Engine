#pragma once

#include "engine.h"
#include <vector>
#include <string>
#include <span>
#include <map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

namespace engine
{
inline std::size_t get_index_timestamp2(float animation_time, std::span<const float> timestamps)
{
    animation_time = std::min(animation_time, timestamps.back());
    for (auto i = 0; i < timestamps.size() - 1; i++)
    {
        if (animation_time <= timestamps[i + 1])
        {
            return i;
        }
    }
    assert(false);
    return 0;
};

inline glm::quat compute_animation_rotation2(std::span<const glm::quat> channel, std::span<const float> timestamps, float animation_time)
{
    if (timestamps.empty())
    {
        return {};
    }
    const auto timestamp_idx_prev = get_index_timestamp2(animation_time, timestamps);
    const auto timestamp_idx_next = timestamp_idx_prev + 1;

    const auto timestamp_prev = timestamps[timestamp_idx_prev];
    const auto timestamp_next = timestamps[timestamp_idx_next];
    const auto interpolation_value = (animation_time - timestamp_prev) / (timestamp_next - timestamp_prev);

    const auto& data_prev = channel[timestamp_idx_prev];
    const auto& data_next = channel[timestamp_idx_next];

    const auto slerp = glm::slerp(data_prev, data_next, interpolation_value);
    return slerp;
}
inline glm::vec3 compute_animation_translation2(const glm::vec3* channel, float animation_time)
{

}
inline glm::vec3 compute_animation_scale2(const glm::vec3* channel, float animation_time)
{

}

}