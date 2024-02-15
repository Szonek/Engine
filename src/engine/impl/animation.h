#pragma once

#include "engine.h"
#include <vector>
#include <string>

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
};
}