#pragma once
#include "mesh_defs.h"

#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/base/memory/unique_ptr.h"

#include <deque>
#include <unordered_map>

namespace engine
{
class PlayBackJob
{
public:
    PlayBackJob(ozz::animation::Animation* anim, std::size_t num_joints, float weight);

    bool update(float dt, ozz::span<ozz::math::SoaTransform> output);

    std::string get_animation_name() const { return animation_->name(); }

private:
    const ozz::animation::Animation* animation_ = nullptr;
    ozz::unique_ptr<ozz::animation::SamplingJob::Context> context_; // ToDo: cache it and reuse?
    float time_ = 0.0f;
    float weight = 0.0f;
};

class AnimationController
{
    friend class Skin;
public:
    AnimationController(Skin* skin);

    void update(float dt);

    bool add_animation(const AnimationClipDesc& animation_clip);
    bool play(const std::string& animation_name, std::size_t layer_id, float weight);
    bool blend_to(const std::string& animation_name, std::size_t layer_id, float weight, float duration);
    bool is_playing(const std::string& animation_name) const;

private:
    Skin* skin_ = nullptr;
    std::unordered_map<std::string, ozz::unique_ptr<ozz::animation::Animation>> animations_;

    std::unordered_map<std::size_t, PlayBackJob> jobs_;
};

} // namespace engine