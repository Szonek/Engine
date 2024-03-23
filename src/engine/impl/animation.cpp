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

    inline glm::quat compute_animation_rotation(const engine::AnimationChannelData<glm::quat>& channel, float animation_time)
    {
        if (channel.timestamps.empty())
        {
            return{ };
        }
        const auto timestamp_idx_prev = get_index_timestamp(animation_time, channel.timestamps);
        const auto timestamp_idx_next = timestamp_idx_prev + 1;

        const auto timestamp_prev = channel.timestamps[timestamp_idx_prev];
        const auto timestamp_next = channel.timestamps[timestamp_idx_next];
        const auto interpolation_value = (animation_time - timestamp_prev) / (timestamp_next - timestamp_prev);

        const auto& data_prev = channel.data[timestamp_idx_prev];
        const auto& data_next = channel.data[timestamp_idx_next];

        const auto slerp = glm::slerp(data_prev, data_next, interpolation_value);
        const auto rotation = glm::eulerAngles(slerp);

        return rotation;
    };

    inline glm::vec3 compute_animation_translation_or_scale(const engine::AnimationChannelData<glm::vec3>& channel, float animation_time, float default_ret_value)
    {
        if (channel.timestamps.empty())
        {
            return glm::vec3 { default_ret_value, default_ret_value, default_ret_value };
        }
        const auto timestamp_idx_prev = get_index_timestamp(animation_time, channel.timestamps);
        const auto timestamp_idx_next = timestamp_idx_prev + 1;

        const auto& timestamp_prev = channel.timestamps[timestamp_idx_prev];
        const auto& timestamp_next = channel.timestamps[timestamp_idx_next];
        const auto interpolation_value = (animation_time - timestamp_prev) / (timestamp_next - timestamp_prev);

        const auto& data_prev = channel.data[timestamp_idx_prev];
        const auto& data_next = channel.data[timestamp_idx_next];

        const auto lerp = glm::mix(data_prev, data_next, interpolation_value);
        return lerp;
    };

    inline glm::vec3 compute_animation_translation(const engine::AnimationChannelData<glm::vec3>& channel, float animation_time)
    {
        return compute_animation_translation_or_scale(channel, animation_time, 0.0f);
    }

    inline glm::vec3 compute_animation_scale(const engine::AnimationChannelData<glm::vec3>& channel, float animation_time)
    {
        return compute_animation_translation_or_scale(channel, animation_time, 1.0f);
    }

}// namespace anonymous

engine::AnimationClip::AnimationClip(const engine_animation_clip_desc_t& desc)
{
    auto fill_timestamp_data_to_vector = [](std::vector<float>& timestampts, const engine_animation_channel_t& channel)
    {
        timestampts.resize(channel.timestamps_count);
        for (std::size_t i = 0; i < timestampts.size(); i++)
        {
            timestampts[i] = channel.timestamps[i];
        }
    };

    auto fill_animation_node_data = [this, &fill_timestamp_data_to_vector](const engine_animation_channel_t& channel)
    {
        auto& node = nodes_[channel.target_node_idx];

        switch (channel.type)
        {
        case ENGINE_ANIMATION_CHANNEL_TYPE_TRANSLATION:
        {
            // timestamp
            fill_timestamp_data_to_vector(node.transform.timestamps, channel);
            // data
            const auto data_size = channel.data_count / 3;
            node.transform.data.reserve(data_size);
            for (std::size_t i = 0; i < data_size; i++)
            {
                const auto idx = i * 3;
                const auto& d = channel.data;
                node.transform.data.push_back(glm::vec3(d[idx], d[idx+1], d[idx+2]));
            }
            break;
        }
        case ENGINE_ANIMATION_CHANNEL_TYPE_ROTATION:
        {
            // timestamp
            fill_timestamp_data_to_vector(node.rotation.timestamps, channel);
            // data
            const auto data_size = channel.data_count / 4;
            node.rotation.data.reserve(data_size);
            for (std::size_t i = 0; i < data_size; i++)
            {
                const auto idx = i * 4;
                const auto& d = channel.data;
                // w, x, y, z
                node.rotation.data.push_back(glm::quat(d[idx+3], d[idx], d[idx + 1], d[idx + 2]));
            }
            break;
        }
        case ENGINE_ANIMATION_CHANNEL_TYPE_SCALE:
        {
            // timestamp
            fill_timestamp_data_to_vector(node.scale.timestamps, channel);
            // data
            const auto data_size = channel.data_count / 3;
            node.scale.data.reserve(data_size);
            for (std::size_t i = 0; i < data_size; i++)
            {
                const auto idx = i * 3;
                const auto& d = channel.data;
                node.scale.data.push_back(glm::vec3(d[idx], d[idx + 1], d[idx + 2]));
            }
            break;
        }
        default:
            assert(false && !"unknown animation channel type!");
        }
    };

    for (std::uint32_t i = 0; i < desc.channels_count; i++)
    {
        const auto& channel = desc.channels[i];
        fill_animation_node_data(channel);
    }

    // find duration
    auto get_last_element_if_bigger = [](auto& duration, const auto& ts)
    {
        if (!ts.empty())
        {
            duration = ts.back() > duration ? ts.back() : duration;
        }
    };
    for (const auto& [idx, data] : nodes_)
    {
        get_last_element_if_bigger(duration_, data.transform.timestamps);
        get_last_element_if_bigger(duration_, data.scale.timestamps);
        get_last_element_if_bigger(duration_, data.rotation.timestamps);
    }

}

float engine::AnimationClip::get_duration() const
{
    return duration_;
}

void engine::AnimationClip::compute_animation_model_matrix(std::span<glm::mat4> skeleton_data, float animation_timer) const
{
    for (const auto& [node_idx, anim_data] : nodes_)
    {
        const auto transform = compute_animation_translation(anim_data.transform, animation_timer);
        const auto scale = compute_animation_scale(anim_data.scale, animation_timer);
        const auto rotate = compute_animation_rotation(anim_data.rotation, animation_timer);
        const auto anim_matrix = compute_model_matrix(transform, rotate, scale);
        skeleton_data[node_idx] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 1.0, 0.0));
        skeleton_data[node_idx] *= anim_matrix;

    }
}