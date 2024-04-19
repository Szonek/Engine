#include "animation.h"

namespace
{
inline std::size_t get_index_timestamp(float animation_time, std::span<const float> timestamps)
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
}   // namespace anonymous

glm::quat engine::compute_animation_rotation(std::span<const glm::quat> channel, std::span<const float> timestamps, float animation_time)
{
    if (timestamps.empty() || channel.empty())
    {
        return glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
    }
    const auto timestamp_idx_prev = get_index_timestamp(animation_time, timestamps);
    const auto timestamp_idx_next = timestamp_idx_prev + 1;

    const auto timestamp_prev = timestamps[timestamp_idx_prev];
    const auto timestamp_next = timestamps[timestamp_idx_next];
    const auto interpolation_value = (animation_time - timestamp_prev) / (timestamp_next - timestamp_prev);

    const auto data_prev = channel[timestamp_idx_prev];
    const auto data_next = channel[timestamp_idx_next];

    const auto slerp = glm::slerp(data_prev, data_next, interpolation_value);
    return glm::normalize(slerp);
}

glm::vec3 engine::compute_animation_translation(std::span<const glm::vec3> channel, std::span<const float> timestamps, float animation_time)
{
    if (timestamps.empty() || channel.empty())
    {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }
    const auto timestamp_idx_prev = get_index_timestamp(animation_time, timestamps);
    const auto timestamp_idx_next = timestamp_idx_prev + 1;

    const auto timestamp_prev = timestamps[timestamp_idx_prev];
    const auto timestamp_next = timestamps[timestamp_idx_next];
    const auto interpolation_value = (animation_time - timestamp_prev) / (timestamp_next - timestamp_prev);

    const auto& data_prev = channel[timestamp_idx_prev];
    const auto& data_next = channel[timestamp_idx_next];

    const auto lerp = glm::mix(data_prev, data_next, interpolation_value);
    return lerp;
}

glm::vec3 engine::compute_animation_scale(std::span<const glm::vec3> channel, std::span<const float> timestamps, float animation_time)
{
    if (timestamps.empty() || channel.empty())
    {
        return glm::vec3(1.0f, 1.0f, 1.0f);
    }
    const auto timestamp_idx_prev = get_index_timestamp(animation_time, timestamps);
    const auto timestamp_idx_next = timestamp_idx_prev + 1;

    const auto timestamp_prev = timestamps[timestamp_idx_prev];
    const auto timestamp_next = timestamps[timestamp_idx_next];
    const auto interpolation_value = (animation_time - timestamp_prev) / (timestamp_next - timestamp_prev);

    const auto& data_prev = channel[timestamp_idx_prev];
    const auto& data_next = channel[timestamp_idx_next];

    const auto lerp = glm::mix(data_prev, data_next, interpolation_value);
    return lerp;
}
