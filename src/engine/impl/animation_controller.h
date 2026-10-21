#pragma once
#include "mesh_defs.h"
#include "components/animation_controller_component.h"

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/memory/unique_ptr.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>

#include <optional>
#include <unordered_map>
#include <functional>
#include <memory>
#include <utility>

namespace engine
{

class Skin;
struct SkinPose;

enum class LayerBlendMode
{
    eOverride,
    eAdditive
};

struct AnimationEvent
{
    std::uint32_t id = 0;
    engine_animation_event_t ev;

    // require for std::sort
    bool operator<(const AnimationEvent& other) const
    {
        return ev.trigger_time < other.ev.trigger_time;
    }
};

using AnimationTimeline = std::vector<AnimationEvent>;

struct CompiledAnimationData
{
    ozz::unique_ptr<ozz::animation::Animation> override_animation;
    ozz::unique_ptr<ozz::animation::Animation> additive_animation;
};

// A model's animation clips compiled for one skin. Shared by every controller of that skin: the "animation set"
// asset in the application's atlas, named "<model path>#animations".
class AnimationLibrary
{
public:
    explicit AnimationLibrary(const Skin& skin);

    bool add_animation(const AnimationClipDesc& animation_clip, const Skin& skin);
    bool is_compatible_with(const Skin& skin) const;
    const std::unordered_map<std::string, std::shared_ptr<const CompiledAnimationData>>& animations() const;

private:
    std::vector<std::string> joint_names_;
    std::unordered_map<std::string, std::shared_ptr<const CompiledAnimationData>> animations_;
};

class SamplingJob
{
public:
    // holds the skeleton's rest pose until it samples an animation: a cross-fade on a layer where nothing played yet
    // fades from it
    SamplingJob(const ozz::animation::Skeleton& skeleton);
    
    void start(const ozz::animation::Animation* anim, const AnimationTimeline* timeline);
    bool update(float dt, bool looping);
    void reset();

    ozz::span<ozz::math::SoaTransform> get_output();

    std::string get_name() const;
    bool is_playing() const;

private:
    float time_ = 0.0f;
    const ozz::animation::Animation* animation_ = nullptr;
    const AnimationTimeline* animation_timeline_ = nullptr;
    std::size_t timeline_next_event_id_ = 0;
    ozz::unique_ptr<ozz::animation::SamplingJob::Context> context_; // ToDo: cache it and reuse?
    ozz::vector<ozz::math::SoaTransform> output_;
};

// Plays animations on one instance of a skin: every update writes the blended result into that instance's pose.
class AnimationController
{
public:
    AnimationController(const Skin* skin, std::shared_ptr<const AnimationLibrary> animation_library);
    AnimationController(const AnimationController&) = delete;
    AnimationController(AnimationController&&) = delete;
    AnimationController& operator=(const AnimationController&) = delete;
    AnimationController& operator=(AnimationController&&) = delete;
    ~AnimationController();

    // how many controllers exist (they are owned by their game objects, so this shows leaks)
    static std::size_t get_live_count();

    void update(float dt, SkinPose& pose);
    const Skin* get_skin() const { return skin_; }
    bool set_paused(bool paused);

    // Replaces the layers with the component's setup (layer id = index). Keeps the animations and their events.
    // false (and logs why) when a layer can't be set up as described, e.g. an unknown joint.
    bool configure_layers(const engine_animation_controller_component_t& component);
    bool has_layer(std::size_t id) const;
    bool set_layer_weight(std::size_t id, float weight);
    bool set_layer_playback_rate(std::size_t id, float playback_rate);
    bool set_layer_looping(std::size_t id, bool looping);

    bool play(const std::string& animation_name, std::size_t layer_id);
    bool cross_fade_to(const std::string& animation_name, std::size_t layer_id, float duration);
    bool is_playing(const std::string& animation_name) const;
    bool is_playing(const std::string& animation_name, std::size_t layer_id) const;
    float get_duration(const std::string& animation_name) const;

    std::pair<bool, std::uint32_t> add_event(const std::string& animation_name, const engine_animation_event_t& ev);
    bool remove_event(const std::string& animation_name, std::uint32_t id);

private:
    bool add_layer(std::size_t id, float weight, LayerBlendMode mode, bool looping);
    bool set_layer_joint_weight(std::size_t id, const std::string& joint_name, float weight);

    struct AnimationData
    {
        std::shared_ptr<const CompiledAnimationData> compiled;
        std::vector<AnimationEvent> timeline;
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
        float playback_rate = 1.0f;
        LayerBlendMode mode;
        bool looping = false;
        SamplingJob animation_a; // base animation
        SamplingJob animation_b; // cross_fade_to
        std::optional<CrossFadeInfo> cross_fade_to;
        std::optional<ozz::vector<ozz::math::SimdFloat4>> joint_weights;  // used for mask
        ozz::vector<ozz::math::SoaTransform> output;

        AnimationLayer(std::size_t id, float weight, const ozz::animation::Skeleton& skeleton, LayerBlendMode mode)
            : id(id)
            , weight(weight)
            , mode(mode)
            , animation_a(skeleton)
            , animation_b(skeleton)
            , output(skeleton.num_soa_joints())
        { }
    };
private:
    const Skin* skin_ = nullptr;
    bool paused_ = false;
    std::unordered_map<std::string, AnimationData> animations_;

    std::unordered_map<std::size_t, AnimationLayer> layers_;
};

} // namespace engine