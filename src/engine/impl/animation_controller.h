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
    
    void start(ozz::animation::Animation* anim);
    bool update(float dt);
    void reset();

    ozz::span<ozz::math::SoaTransform> get_output();

    std::string get_name() const;
    bool is_playing() const;

private:
    float time_ = 0.0f;
    const ozz::animation::Animation* animation_ = nullptr;
    ozz::unique_ptr<ozz::animation::SamplingJob::Context> context_; // ToDo: cache it and reuse?
    ozz::vector<ozz::math::SoaTransform> output_;
};

enum class LayerBlendMode
{
    eOverride,
    eAdditive
};

class AnimationController
{
    friend class Skin;
public:
    AnimationController(Skin* skin);

    void update(float dt);

    bool add_layer(std::size_t id, float weight);
    bool remove_layer(std::size_t id);
    bool has_layer(std::size_t id) const;
    bool set_layer_weight(std::size_t id, float weight);
    bool set_layer_mode(std::size_t id, LayerBlendMode mode);

    bool add_animation(const AnimationClipDesc& animation_clip);
    bool play(const std::string& animation_name, std::size_t layer_id);
    bool cross_fade_to(const std::string& animation_name, std::size_t layer_id, float duration);
    bool is_playing(const std::string& animation_name) const;

private:
    struct AnimationData
    {
        ozz::unique_ptr<ozz::animation::Animation> override;
        ozz::unique_ptr<ozz::animation::Animation> additive;
    };

    struct CrossFadeInfo
    {
        float duration = 0.0f; // set by user 
        float time = 0.0f; // track current time
    };
    struct AnimationLayer
    {
        std::size_t id;
        float weight;
        LayerBlendMode mode;
        SamplingJob animation_a; // base animation
        SamplingJob animation_b; // cross_fade_to
        std::optional<CrossFadeInfo> cross_fade_to;
        ozz::vector<ozz::math::SoaTransform> output;

        AnimationLayer(std::size_t id, float weight, std::size_t num_joints, LayerBlendMode mode)
            : id(id)
            , weight(weight)
            , mode(mode)
            , animation_a(num_joints)
            , animation_b(num_joints)
            , output((num_joints + 3) / 4)
        { }
        AnimationLayer()
            : AnimationLayer(0, 0.0f, 0, LayerBlendMode::eOverride)
        { }
    };
private:
    Skin* skin_ = nullptr;
    std::unordered_map<std::string, AnimationData> animations_;

    std::unordered_map<std::size_t, AnimationLayer> layers_;
};

} // namespace engine