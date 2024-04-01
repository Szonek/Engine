#pragma once

#include "engine.h"
#include <vector>
#include <string>
#include <span>
#include <map>

#include <glm/glm.hpp>

namespace engine
{
template<typename T>
struct AnimationChannelData
{
    std::vector<float> timestamps;
    std::vector<T> data;
};

class AnimationClip
{
public:
    
    AnimationClip() = default;
    AnimationClip(const engine_animation_clip_create_desc_t& desc);
    AnimationClip(const AnimationClip&) = delete;
    AnimationClip(AnimationClip&& rhs) noexcept = default;
    AnimationClip& operator=(const AnimationClip& rhs) = delete;
    AnimationClip& operator=(AnimationClip&& rhs)  noexcept = default;
    ~AnimationClip() = default;

    float get_duration() const;
    bool compute_animation_model_matrix(std::span<struct TRS> animation_data, float animation_timer) const;

private:
    struct AnimationNodeData
    {
        AnimationChannelData<glm::vec3> transform;
        AnimationChannelData<glm::vec3> scale;
        AnimationChannelData<glm::quat> rotation;
    };

    std::map<std::int32_t, AnimationNodeData> nodes_;
    float duration_ = 0.0f;

};

}