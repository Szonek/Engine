#pragma once

#include "engine.h"
#include <vector>
#include <string>

#include <glm/glm.hpp>


namespace engine
{
struct AnimationChannelData
{
    engine_animation_channel_type_t type = ENGINE_ANIMATION_CHANNEL_TYPE_COUNT;
    std::vector<float> timestamps;
    std::vector<float> data;
};

struct AnimationClipData
{
    std::vector<AnimationChannelData> channels;
    float get_duration() const;  // cache this data
    glm::mat4 compute_animation_model_matrix(const glm::mat4& base_matrix, float animation_timer) const;
};

}