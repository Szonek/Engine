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
class Skin; // Forward declaration

class PlayBackJob
{
public:
    PlayBackJob(ozz::animation::Animation* anim, ozz::animation::SamplingJob::Context& ctx, std::size_t num_joints);

    bool update(float dt, ozz::span<ozz::math::SoaTransform> output);

    std::string get_animation_name() const { return animation_->name(); }

private:
    const ozz::animation::Animation* animation_ = nullptr;
    ozz::animation::SamplingJob::Context* context_;
    float time_ = 0.0f;
};

class AnimationController
{
    friend class Skin;
public:
    AnimationController(Skin* skin);

    void update(float dt);

    bool add_animation(const AnimationClipDesc& animation_clip);
    bool set_layer_id(const std::string& animation_name, std::size_t layer_id);
    bool play(const std::string& animation_name);
    bool is_playing(const std::string& animation_name) const;

private:
    struct animation_desc
    {
        ozz::unique_ptr<ozz::animation::Animation> animation = nullptr;
        std::size_t layer_id = 0;
    };

private:
    Skin* skin_ = nullptr;
    ozz::animation::SamplingJob::Context context_;
    std::unordered_map<std::string, animation_desc> animations_;

    std::unordered_map<std::size_t, PlayBackJob> jobs_;
};

} // namespace engine