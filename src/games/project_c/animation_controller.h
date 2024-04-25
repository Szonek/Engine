#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <map>

namespace project_c
{
template<typename T>
struct AnimationChannelBase
{
    using DataType = T;
    std::vector<T> data;
    std::vector<float> timestamps;
};
using AnimationChannelVec3 = AnimationChannelBase<glm::vec3>;
using AnimationChannelQuat = AnimationChannelBase<glm::quat>;

struct AnimationChannelData
{
    AnimationChannelVec3 translation;
    AnimationChannelVec3 scale;
    AnimationChannelQuat rotation;
};

class AnimationClip
{
public:
    AnimationClip() = default;
    AnimationClip(engine_scene_t scene, std::map<engine_game_object_t, AnimationChannelData>&& data)
        : scene_(scene)
        , channels_(std::move(data))
        , duration_(compute_duration())
    {
    }

public:
    void update(float dt)
    {
        animation_dt_ += dt;

        for (const auto& [go, channel] : channels_)
        {
            auto tc = engineSceneGetTransformComponent(scene_, go);
            if (!channel.translation.timestamps.empty())
            {
                const auto translation = interpolate(channel.translation.timestamps, channel.translation.data, animation_dt_);
                tc.position[0] = translation.x;
                tc.position[1] = translation.y;
                tc.position[2] = translation.z;
            }
            if (!channel.scale.timestamps.empty())
            {
                const auto scale = interpolate(channel.scale.timestamps, channel.scale.data, animation_dt_);
                tc.scale[0] = scale.x;
                tc.scale[1] = scale.y;
                tc.scale[2] = scale.z;
            }
            if (!channel.rotation.timestamps.empty())
            {
                const auto rotation = interpolate(channel.rotation.timestamps, channel.rotation.data, animation_dt_);
                tc.rotation[0] = rotation.x;
                tc.rotation[1] = rotation.y;
                tc.rotation[2] = rotation.z;
                tc.rotation[3] = rotation.w;
            }
            engineSceneUpdateTransformComponent(scene_, go, &tc);
        }

        if (animation_dt_ > duration_)
        {
            animation_dt_ = 0.0f;
        }
    }

private:
    inline float compute_duration() const
    {
        float max_duration = 0.0f;
        for (const auto& [go, channel_data] : channels_)
        {
            if (!channel_data.translation.timestamps.empty())
            {
                max_duration = std::max(max_duration, channel_data.translation.timestamps.back());
            }
            if (!channel_data.scale.timestamps.empty())
            {
                max_duration = std::max(max_duration, channel_data.scale.timestamps.back());
            }
            if (!channel_data.rotation.timestamps.empty())
            {
                max_duration = std::max(max_duration, channel_data.rotation.timestamps.back());
            }
        }
        return max_duration;
    }

    inline std::size_t get_index_timestamp(float animation_time, std::span<const float> timestamps) const
    {
        assert(!timestamps.empty());
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

    glm::quat interpolate(std::span<const float> timestamps, std::span<const glm::quat> channel, float animation_time) const
    {
        assert(!channel.empty());
        assert(!timestamps.empty());
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

    glm::vec3 interpolate(std::span<const float> timestamps, std::span<const glm::vec3> channel, float animation_time) const
    {
        assert(!channel.empty());
        assert(!timestamps.empty());
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


private:
    engine_scene_t scene_;
    std::map<engine_game_object_t, AnimationChannelData> channels_;
    float duration_ = 0.0f;
    float animation_dt_ = 0.0f;
};

class AnimationCollection
{
public:
    void add_animation_clip(const std::string& name, const AnimationClip& clip)
    {
        animation_clips_[name] = clip;
    }

    const AnimationClip& get_animation_clip(const std::string& name) const
    {
        return animation_clips_.at(name);
    }

    AnimationClip& get_animation_clip(const std::string& name)
    {
        return animation_clips_.at(name);
    }

    bool has_animation_clip(const std::string& name) const
    {
        return animation_clips_.find(name) != animation_clips_.end();
    }

    void remove_animation_clip(const std::string& name)
    {
        animation_clips_.erase(name);
    }

    void clear()
    {
        animation_clips_.clear();
    }

    bool is_empty() const
    {
        return animation_clips_.empty();
    }

private:
    std::map<std::string, AnimationClip> animation_clips_;
};

class AnimationController
{
public:

    bool has_animations_clips() const
    {
        return !collection_.is_empty();
    }

    void add_animation_clip(const std::string& name, const AnimationClip& clip)
    {
        collection_.add_animation_clip(name, clip);
    }

    void set_active_animation(const std::string& name)
    {
        current_clip_ = &collection_.get_animation_clip(name);

    }

    void update(float dt)
    {
        if (current_clip_)
        {
            current_clip_->update(dt);
        }
    }

private:
    AnimationClip* current_clip_ = nullptr;
    AnimationCollection collection_;
};

} // namespace project_c