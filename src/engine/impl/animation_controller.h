#pragma once
#include "mesh_defs.h"

#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/base/memory/unique_ptr.h"

#include <unordered_map>

namespace engine
{
class AnimationController
{
    friend class Skin;
public:
    AnimationController(Skin* skin);

    bool add_animation(const AnimationClipDesc& animation_clip);
    bool playback(const std::string& animation_name, float delta_time);

private:
    Skin* skin_ = nullptr;
    ozz::animation::SamplingJob::Context context_;
    std::unordered_map<std::string, ozz::unique_ptr<ozz::animation::Animation>> animations_;

    float temp_playback_ratio_ = 0.0f;
};

} // namespace engine