#pragma once
#include "mesh_defs.h"

#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/base/containers/vector.h"

#include <optional>
#include <deque>
#include <unordered_map>

namespace engine
{

class SamplingJob
{
public:
    SamplingJob(std::size_t num_joints);
    
    void start(ozz::animation::Animation* anim, float weight);
    void reset();

    void update(float dt);
    
    std::string get_name() const;
    bool is_finished() const;
    
private:
    float time_ = 0.0f;
    float weight_ = 0.0f;
    const ozz::animation::Animation* animation_ = nullptr;
    ozz::unique_ptr<ozz::animation::SamplingJob::Context> context_; // ToDo: cache it and reuse?
    ozz::vector<ozz::math::SoaTransform> output_;
};

class AnimationController
{
    friend class Skin;
public:
    AnimationController(Skin* skin);

    void update(float dt);

    bool add_layer(std::size_t id, float weight);
    bool remove_layer(std::size_t id);
    bool set_layer_weight(std::size_t id, float weight);

    bool add_animation(const AnimationClipDesc& animation_clip);
    bool play(const std::string& animation_name, std::size_t layer_id, float weight);
    bool blend_to(const std::string& animation_name, std::size_t layer_id, float weight, float duration);
    bool is_playing(const std::string& animation_name) const;

private:
    struct BlendInfo
    {
        float duration = 0.0f;
    };
    struct AnimationLayer
    {
        std::size_t id;
        float weight;
        SamplingJob animation_a; // base animation
        SamplingJob animation_b; // blend_to
        std::optional<BlendInfo> blend_to;
        
        AnimationLayer(std::size_t id, float weight, std::size_t num_joints)
            : id(id)
            , weight(weight)
            , animation_a(num_joints)
            , animation_b(num_joints)
        { }
        AnimationLayer()
            : AnimationLayer(0, 0.0f, 0)
        { }
    };
private:
    Skin* skin_ = nullptr;
    std::unordered_map<std::string, ozz::unique_ptr<ozz::animation::Animation>> animations_;

    std::unordered_map<std::size_t, AnimationLayer> layers_;
};

} // namespace engine