#include "animation.h"
#include "math_helpers.h"
#include <cassert>
#include <array>
#include <algorithm>
#include <span>


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

inline glm::quat compute_animation_rotation(const engine::AnimationChannelData& channel, float animation_time)
{
    const auto timestamp_idx_prev = get_index_timestamp(animation_time, channel.timestamps);
    const auto timestamp_idx_next = timestamp_idx_prev + 1;

    const auto timestamp_prev = channel.timestamps[timestamp_idx_prev];
    const auto timestamp_next = channel.timestamps[timestamp_idx_next];
    const auto interpolation_value = (animation_time - timestamp_prev) / (timestamp_next - timestamp_prev);

    const auto rotation_quaternions = [&]()
    {
        std::array<float, 4> data_rot_prev = {};
        std::array<float, 4> data_rot_next = {};
        for (auto i = 0; i < data_rot_prev.size(); i++)
        {
            data_rot_prev[i] = channel.data[timestamp_idx_prev * 4 + i];
            data_rot_next[i] = channel.data[timestamp_idx_next * 4 + i];
        }

        std::pair<glm::quat, glm::quat> ret =
        {
            glm::quat(data_rot_prev[3], data_rot_prev[0], data_rot_prev[1], data_rot_prev[2]),
            glm::quat(data_rot_next[3], data_rot_next[0], data_rot_next[1], data_rot_next[2])
        };
        return ret;
    }();

    const auto slerp = glm::slerp(rotation_quaternions.first, rotation_quaternions.second, interpolation_value);
    const auto rotation = glm::eulerAngles(slerp);

    return rotation;
};

inline glm::vec3 compute_animation_translation_or_scale(const engine::AnimationChannelData& channel, float animation_time)
{
    const auto timestamp_idx_prev = get_index_timestamp(animation_time, channel.timestamps);
    const auto timestamp_idx_next = timestamp_idx_prev + 1;

    const auto timestamp_prev = channel.timestamps[timestamp_idx_prev];
    const auto timestamp_next = channel.timestamps[timestamp_idx_next];
    const auto interpolation_value = (animation_time - timestamp_prev) / (timestamp_next - timestamp_prev);

    const auto translation_vectors = [&]()
    {
        std::array<float, 3> data_prev = {};
        std::array<float, 3> data_next = {};
        for (auto i = 0; i < data_prev.size(); i++)
        {
            data_prev[i] = channel.data[timestamp_idx_prev * 3 + i];
            data_next[i] = channel.data[timestamp_idx_next * 3 + i];
        }

        std::pair<glm::vec3, glm::vec3> ret =
        {
            glm::vec3(data_prev[0], data_prev[1], data_prev[2]),
            glm::vec3(data_next[0], data_next[1], data_next[2])
        };
        return ret;
    }();

    const auto lerp = glm::mix(translation_vectors.first, translation_vectors.second, interpolation_value);
    return lerp;
};

}

float engine::AnimationClipData::get_duration() const
{
    float max = 0.0f;
    for (const auto& ch : channels)
    {
        max = ch.timestamps.back() > max ? ch.timestamps.back() : max;
    }
    return max;
}

glm::mat4 engine::AnimationClipData::compute_animation_model_matrix(const glm::mat4& base_matrix, float animation_timer) const
{
    glm::vec3 transform{ 0.0f };
    glm::vec3 scale{ 1.0f, 1.0f,1.0f };
    glm::quat rotate{};
    for (const auto& ch : channels)
    {
        if (ch.type == ENGINE_ANIMATION_CHANNEL_TYPE_ROTATION)
        {
            rotate = compute_animation_rotation(ch, animation_timer);
        }
        else if (ch.type == ENGINE_ANIMATION_CHANNEL_TYPE_SCALE)
        {
            scale = compute_animation_translation_or_scale(ch, animation_timer);
        }
        else if (ch.type == ENGINE_ANIMATION_CHANNEL_TYPE_TRANSLATION)
        {
            transform = compute_animation_translation_or_scale(ch, animation_timer);
        }
    }
    const auto anim_matrix = compute_model_matrix(transform, rotate, scale);
    return base_matrix * anim_matrix;
}
