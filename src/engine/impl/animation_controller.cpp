#include "animation_controller.h"
#include "skin.h"
#include "logger.h"
#include "profiler.h"

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/additive_animation_builder.h>

#include <ozz/animation/runtime/skeleton_utils.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/blending_job.h>

#include <algorithm>
#include <cstring>
#include <atomic>
#include <stdexcept>
#include <format>
#include <cmath>

namespace
{
// Helper functor used to set weights while traversing joints hierarchy.
struct WeightSetupIterator 
{
    WeightSetupIterator(ozz::vector<ozz::math::SimdFloat4>& _weights, float _weight_setting)
        : weights(_weights), weight_setting(_weight_setting)
    {
    }
    void operator()(std::int32_t _joint, int)
    {
        ozz::math::SimdFloat4& soa_weight = weights.at(_joint / 4);
        soa_weight = ozz::math::SetI(
            soa_weight, ozz::math::simd_float4::Load1(weight_setting),
            _joint % 4);
    }
    ozz::vector<ozz::math::SimdFloat4>& weights;
    float weight_setting;
};
}

engine::AnimationLibrary::AnimationLibrary(const Skin& skin)
{
    joint_names_.reserve(skin.get_skeleton().num_joints());
    for (int i = 0; i < skin.get_skeleton().num_joints(); ++i)
    {
        joint_names_.emplace_back(skin.get_skeleton().joint_names()[i]);
    }
}

bool engine::AnimationLibrary::add_animation(const AnimationClipDesc& animation_clip, const Skin& skin)
{
    ENGINE_PROFILE_SECTION;
    if (!is_compatible_with(skin))
    {
        log::log(log::LogLevel::eError, "Cannot compile animation data for an incompatible skin skeleton.\n");
        return false;
    }
    ozz::animation::offline::RawAnimation raw_animation;
    raw_animation.name = animation_clip.name;
    raw_animation.duration = animation_clip.duration;

    if (raw_animation.duration <= 0.0f)
    {
        log::log(log::LogLevel::eError, std::format("Invalid animation duration: {} for animation '{}'.\n", raw_animation.duration, raw_animation.name).c_str());
        raw_animation.duration = 0.00001f;
    }
    if (raw_animation.name.empty())
    {
        log::log(log::LogLevel::eError, "Animation name cannot be empty.\n");
        return false;
    }
    if (animations_.find(animation_clip.name) != animations_.end())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' already exists in the controller.\n", raw_animation.name).c_str());
        return false;
    }
    if (animation_clip.channels.empty())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' has no channels.\n", raw_animation.name).c_str());
        return false;
    }
    raw_animation.tracks.resize(skin.get_skeleton().num_joints());

    for (const auto& channel : animation_clip.channels)
    {
        const auto joint_idx = ozz::animation::FindJoint(skin.get_skeleton(), channel.joint_name.c_str());
        if (joint_idx < 0)
        {
            log::log(log::LogLevel::eError, std::format("Joint '{}' of animation '{}' not found in the skin.\n", channel.joint_name, animation_clip.name).c_str());
            return false;
        }
        auto& track = raw_animation.tracks[joint_idx];
        
        switch (channel.type)
        {
            case AnimationChannelType::eTranslation:
            {
                for (std::size_t i = 0; i < channel.timestamps.size(); ++i)
                {
                    ozz::animation::offline::RawAnimation::TranslationKey key;
                    key.time = channel.timestamps.at(i);
                    key.value = ozz::math::Float3(channel.data.at(i * 3), channel.data.at(i * 3 + 1), channel.data.at(i * 3 + 2));
                    track.translations.push_back(std::move(key));
                }
                break;
            }
            case AnimationChannelType::eRotation:
            {
                for (std::size_t i = 0; i < channel.timestamps.size(); ++i)
                {
                    ozz::animation::offline::RawAnimation::RotationKey key;
                    key.time = channel.timestamps.at(i);
                    key.value = ozz::math::Quaternion(channel.data.at(i * 4), channel.data.at(i * 4 + 1), channel.data.at(i * 4 + 2), channel.data.at(i * 4 + 3));
                    track.rotations.push_back(std::move(key));
                }
                break;
            }
            case AnimationChannelType::eScale:
            {
                for (std::size_t i = 0; i < channel.timestamps.size(); ++i)
                {
                    ozz::animation::offline::RawAnimation::ScaleKey key;
                    key.time = channel.timestamps.at(i);
                    key.value = ozz::math::Float3(channel.data.at(i * 3), channel.data.at(i * 3 + 1), channel.data.at(i * 3 + 2));
                    track.scales.push_back(std::move(key));
                }
                break;
            }
            case AnimationChannelType::eUnknown:
            {
                break;
            }
        }
    }

    if (!raw_animation.Validate())
    {
        throw std::runtime_error("Invalid raw animation data.");
    }


    const auto num_tracks = raw_animation.tracks.size();
    log::log(log::LogLevel::eTrace, std::format("Adding animation '{}' with {} tracks to controller.\n", raw_animation.name, num_tracks).c_str());
    
    ozz::animation::offline::RawAnimation raw_animation_additive;
    ozz::animation::offline::AdditiveAnimationBuilder additive_builder{};
    const auto res = additive_builder(raw_animation, &raw_animation_additive);
    if (!res)
    {
        log::log(log::LogLevel::eError, std::format("Failed to build additive animation '{}'.\n", raw_animation.name).c_str());
        return false;
    }
    auto compiled = std::make_shared<CompiledAnimationData>();
    ozz::animation::offline::AnimationBuilder builder;
    compiled->override_animation = builder(raw_animation);
    compiled->additive_animation = builder(raw_animation_additive);
    if (!compiled->override_animation || !compiled->additive_animation)
    {
        log::log(log::LogLevel::eError, std::format("Failed compiling animation '{}'.\n", raw_animation.name).c_str());
        return false;
    }
    animations_.emplace(raw_animation.name, std::move(compiled));
    return true;
}

bool engine::AnimationLibrary::is_compatible_with(const Skin& skin) const
{
    if (joint_names_.size() != static_cast<std::size_t>(skin.get_skeleton().num_joints()))
    {
        return false;
    }
    for (std::size_t i = 0; i < joint_names_.size(); ++i)
    {
        if (joint_names_[i] != skin.get_skeleton().joint_names()[i])
        {
            return false;
        }
    }
    return true;
}

const std::unordered_map<std::string, std::shared_ptr<const engine::CompiledAnimationData>>& engine::AnimationLibrary::animations() const
{
    return animations_;
}

namespace
{
std::atomic<std::size_t> G_LIVE_ANIMATION_CONTROLLERS = 0;
}  // namespace

engine::AnimationController::AnimationController(const Skin* skin, std::shared_ptr<const AnimationLibrary> animation_library)
    : skin_(skin)
{
    ENGINE_PROFILE_SECTION;
    if (!skin_ || !animation_library)
    {
        throw std::invalid_argument("An animation controller needs a skin and an animation set.");
    }
    if (!animation_library->is_compatible_with(*skin_))
    {
        throw std::invalid_argument("Animation library is incompatible with the skin skeleton.");
    }
    for (const auto& [name, compiled] : animation_library->animations())
    {
        animations_.emplace(name, AnimationData{ compiled, {} });
    }
    // no layers until configure_layers
    G_LIVE_ANIMATION_CONTROLLERS++;
}

engine::AnimationController::~AnimationController()
{
    G_LIVE_ANIMATION_CONTROLLERS--;
}

std::size_t engine::AnimationController::get_live_count()
{
    return G_LIVE_ANIMATION_CONTROLLERS;
}

void engine::AnimationController::update(float dt, SkinPose& pose)
{
    ENGINE_PROFILE_SECTION;
    if (dt == 0.0f)
    {
        return;
    }
    assert(dt > 0.0f);
    if (paused_)
    {
        return;
    }

    ozz::vector<ozz::animation::BlendingJob::Layer> executed_layers;
    executed_layers.reserve(layers_.size());

    ozz::vector<ozz::animation::BlendingJob::Layer> executed_additive_layers;
    executed_additive_layers.reserve(layers_.size());

    for (auto& [_, layer] : layers_)
    {
        const bool has_played_a = layer.animation_a.update(dt * layer.playback_rate, layer.looping);
        const bool has_played_b = layer.animation_b.update(dt * layer.playback_rate, layer.looping);
        // if both animation have not played then skip current layer
        if (!has_played_a && !has_played_b)
        {
            continue;
        }

        float anim_b_weight = 0.0f;
        if (layer.cross_fade_to)
        {
            assert(has_played_b == true);
            layer.cross_fade_to->time += dt/1000.0f;
            anim_b_weight = std::clamp(layer.cross_fade_to->time / layer.cross_fade_to->duration, 0.0f, 1.0f);
        }

        ozz::vector<ozz::animation::BlendingJob::Layer> anim_layers(2);
        {
            anim_layers[0].transform = ozz::make_span(layer.animation_a.get_output());
            anim_layers[0].weight = 1.0f - anim_b_weight;
        }
        {
            anim_layers[1].transform = ozz::make_span(layer.animation_b.get_output());
            anim_layers[1].weight = anim_b_weight;
        }

        // Setups blending job.
        ozz::animation::BlendingJob blend_job;
        blend_job.threshold = ozz::animation::BlendingJob().threshold;
        blend_job.layers = ozz::make_span(anim_layers);
        blend_job.rest_pose = skin_->get_skeleton().joint_rest_poses();
        blend_job.output = ozz::make_span(layer.output);

        // Blends.
        if (!blend_job.Run()) 
        {
            log::log(log::LogLevel::eError, "Failed to blend an animation layer.\n");
        }

        // add to executed layer result
        ozz::animation::BlendingJob::Layer layer_result{};
        layer_result.weight = layer.weight;
        layer_result.transform = ozz::make_span(layer.output);
        if (layer.joint_weights)
        {
            layer_result.joint_weights = ozz::make_span(*layer.joint_weights);
        }
        if (layer.mode == LayerBlendMode::eOverride)
        {
            executed_layers.push_back(layer_result);
        }
        else if (layer.mode == LayerBlendMode::eAdditive)
        {
            executed_additive_layers.push_back(layer_result);
        }
        else
        {
            assert(!"Should never hit here");
        }

        // swap and reset
        if (anim_b_weight >= 1.0f)
        {
            std::swap(layer.animation_a, layer.animation_b);
            layer.animation_b.reset();
            layer.cross_fade_to = std::nullopt;
        }
    }
    // early exit, no work to do
    if (executed_layers.empty() && executed_additive_layers.empty())
    {
        return;
    }

    // cross-layer blend
    ozz::vector<ozz::math::SoaTransform> final_output(skin_->get_skeleton().num_soa_joints());
    ozz::animation::BlendingJob blend_job;
    blend_job.threshold = ozz::animation::BlendingJob().threshold;
    blend_job.layers = ozz::make_span(executed_layers);
    blend_job.additive_layers = ozz::make_span(executed_additive_layers);
    blend_job.rest_pose = skin_->get_skeleton().joint_rest_poses();
    blend_job.output = ozz::make_span(final_output);

    // Blends.
    if (!blend_job.Run())
    {
        log::log(log::LogLevel::eError, "Failed blend cross-layer result.\n");
        throw std::runtime_error("Failed blend cross-layer result.");
    }

    // compute model matrices which will be used during skinning
    ozz::animation::LocalToModelJob ltm_job{};
    ltm_job.skeleton = &skin_->get_skeleton();
    ltm_job.input = ozz::make_span(final_output);
    ltm_job.output = ozz::make_span(pose.models);

    if (!ltm_job.Run())
    {
        log::log(log::LogLevel::eError, "Failed to convert local to model space for skin.\n");
        throw std::runtime_error("Failed to convert local to model space for skin.");
    }
}

bool engine::AnimationController::set_paused(bool paused)
{
    paused_ = paused;
    return true;
}

bool engine::AnimationController::configure_layers(const engine_animation_controller_component_t& component)
{
    layers_.clear();
    if (component.layers_count > ENGINE_ANIMATION_CONTROLLER_MAX_LAYERS)
    {
        log::log(log::LogLevel::eError, std::format("An animation controller can have at most {} layers, got {}.\n", ENGINE_ANIMATION_CONTROLLER_MAX_LAYERS, component.layers_count).c_str());
        return false;
    }
    bool ok = true;
    for (std::uint32_t id = 0; id < component.layers_count; id++)
    {
        const auto& layer = component.layers[id];
        const auto mode = layer.mode == ENGINE_ANIMATION_LAYER_MODE_ADDITIVE ? LayerBlendMode::eAdditive : LayerBlendMode::eOverride;
        add_layer(id, layer.weight, mode, layer.looping);
        if (layer.joint_weights_count > ENGINE_ANIMATION_LAYER_MAX_JOINT_WEIGHTS)
        {
            log::log(log::LogLevel::eError, std::format("Animation layer {} has more than {} joint weights.\n", id, ENGINE_ANIMATION_LAYER_MAX_JOINT_WEIGHTS).c_str());
            ok = false;
            continue;
        }
        for (std::uint32_t i = 0; i < layer.joint_weights_count; i++)
        {
            const auto& joint_weight = layer.joint_weights[i];
            const std::string joint_name(joint_weight.joint_name, strnlen(joint_weight.joint_name, ENGINE_ANIMATION_JOINT_NAME_MAX_LENGTH));
            ok &= set_layer_joint_weight(id, joint_name, joint_weight.weight);
        }
    }
    return ok;
}

bool engine::AnimationController::add_layer(std::size_t id, float weight, LayerBlendMode mode, bool looping)
{
    if (has_layer(id))
    {
        return false;
    }
    auto& layer = layers_.emplace(id, AnimationLayer(id, weight, skin_->get_skeleton(), mode)).first->second;
    layer.looping = looping;
    return true;
}

bool engine::AnimationController::has_layer(std::size_t id) const
{
    return layers_.contains(id);
}

bool engine::AnimationController::set_layer_weight(std::size_t id, float weight)
{
    if (!has_layer(id))
    {
        return false;
    }
    layers_.at(id).weight = weight;
    return true;
}

bool engine::AnimationController::set_layer_playback_rate(std::size_t id, float playback_rate)
{
    if (!has_layer(id))
    {
        log::log(log::LogLevel::eError, std::format("Layer with id: {} does not exist\n", id).c_str());
        return false;
    }
    if (!std::isfinite(playback_rate) || playback_rate <= 0.0f)
    {
        log::log(log::LogLevel::eError, std::format("Animation playback rate must be finite and greater than zero, got: {}\n", playback_rate).c_str());
        return false;
    }

    layers_.at(id).playback_rate = playback_rate;
    return true;
}

bool engine::AnimationController::set_layer_looping(std::size_t id, bool looping)
{
    if (!has_layer(id))
    {
        log::log(log::LogLevel::eError, std::format("Layer with id: {} does not exist\n", id).c_str());
        return false;
    }

    layers_.at(id).looping = looping;
    return true;
}

bool engine::AnimationController::set_layer_joint_weight(std::size_t id, const std::string& joint_name, float weight)
{
    if (!has_layer(id))
    {
        log::log(log::LogLevel::eError, std::format("Layer with id: {} does not exist\n", id).c_str());
        return false;
    }
    if (weight < 0.0f || weight > 1.0f)
    {
        log::log(log::LogLevel::eError, std::format("Joint weight must be between 0 and 1, got: {}\n", weight).c_str());
        return false;
    }

    const auto joint_index = ozz::animation::FindJoint(skin_->get_skeleton(), joint_name.c_str());
    if (joint_index < 0)
    {
        log::log(log::LogLevel::eError, std::format("Joint '{}' not found in the skin.\n", joint_name).c_str());
        return false;
    }

    auto& layer = layers_.at(id);
    if (!layer.joint_weights)
    {
        ozz::vector<ozz::math::SimdFloat4> weights(
            (skin_->get_skeleton().num_joints() + 3) / 4,
            ozz::math::simd_float4::Load1(0.0f));
        layer.joint_weights = std::move(weights);
    }
    ozz::animation::IterateJointsDF(
        skin_->get_skeleton(),
        WeightSetupIterator(*layer.joint_weights, weight),
        joint_index);
    return true;
}

bool engine::AnimationController::play(const std::string& animation_name, std::size_t layer_id)
{
    ENGINE_PROFILE_SECTION;
    if (!layers_.contains(layer_id))
    {
        log::log(log::LogLevel::eError, std::format("Layer with id: {} does not exist\n", layer_id).c_str());
        return false;
    }

    auto it = animations_.find(animation_name);
    if (it == animations_.end())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' not found in the controller.\n", animation_name).c_str());
        return false;
    }
    
    const auto& animation = layers_.at(layer_id).mode == LayerBlendMode::eAdditive
        ? it->second.compiled->additive_animation
        : it->second.compiled->override_animation;
    if (!animation)
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' is null.\n", animation_name).c_str());
        return false;
    }

    layers_.at(layer_id).animation_a.start(animation.get(), &it->second.timeline);
    return true;
}

bool engine::AnimationController::cross_fade_to(const std::string& animation_name, std::size_t layer_id, float duration)
{
    ENGINE_PROFILE_SECTION;
    if (!layers_.contains(layer_id))
    {
        log::log(log::LogLevel::eError, std::format("Layer with id: {} does not exist\n", layer_id).c_str());
        return false;
    }

    auto it = animations_.find(animation_name);
    if (it == animations_.end())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' not found in the controller.\n", animation_name).c_str());
        return false;
    }

    const auto& animation = layers_.at(layer_id).mode == LayerBlendMode::eAdditive
        ? it->second.compiled->additive_animation
        : it->second.compiled->override_animation;
    if (!animation)
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' is null.\n", animation_name).c_str());
        return false;
    }

    auto& layer = layers_.at(layer_id);
    if (layer.animation_b.is_playing())
    {
        std::swap(layer.animation_a, layer.animation_b);
    }
    layer.animation_b.start(animation.get(), &it->second.timeline);
    layer.cross_fade_to = CrossFadeInfo{ duration };
    return true;
}

bool engine::AnimationController::is_playing(const std::string& animation_name) const
{
    ENGINE_PROFILE_SECTION;
    for (const auto& [layer_id, layer] : layers_)
    {
        if (layer.animation_a.is_playing())
        {
            if (layer.animation_a.get_name() == animation_name)
            {
                return true;
            }
        }
        if (layer.animation_b.is_playing())
        {
            if (layer.animation_b.get_name() == animation_name)
            {
                return true;
            }
        }
    }

    return false;
}

bool engine::AnimationController::is_playing(const std::string& animation_name, std::size_t layer_id) const
{
    const auto layer_it = layers_.find(layer_id);
    if (layer_it == layers_.end())
    {
        log::log(log::LogLevel::eError, std::format("Layer with id: {} does not exist\n", layer_id).c_str());
        return false;
    }

    const auto& layer = layer_it->second;
    return (layer.animation_a.is_playing() && layer.animation_a.get_name() == animation_name)
        || (layer.animation_b.is_playing() && layer.animation_b.get_name() == animation_name);
}

float engine::AnimationController::get_duration(const std::string& animation_name) const
{
    auto it = animations_.find(animation_name);
    if (it == animations_.end())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' not found in the controller.\n", animation_name).c_str());
        return 0.0f;
    }
    return it->second.compiled->override_animation->duration();
}

std::pair<bool, std::uint32_t> engine::AnimationController::add_event(const std::string& animation_name, const engine_animation_event_t& ev)
{
    auto it = animations_.find(animation_name);
    if (it == animations_.end())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' not found in the controller.\n", animation_name).c_str());
        return { false, ENGINE_INVALID_OBJECT_HANDLE };
    }
    // event ids are never ENGINE_INVALID_OBJECT_HANDLE
    static std::uint32_t uuid = ENGINE_INVALID_OBJECT_HANDLE + 1; // ToDo: this id mechanism is not ideal, improve it
    AnimationEvent new_ev{};
    new_ev.id = uuid;
    new_ev.ev = ev;

    //ToDo: just use std map/multimap? we could avoid sorting
    auto& timeline = it->second.timeline;
    timeline.push_back(new_ev);
    std::sort(timeline.begin(), timeline.end());
    // bump id so each event has unique value
    uuid++;
    return { true, new_ev.id };
}

bool engine::AnimationController::remove_event(const std::string& animation_name, std::uint32_t id)
{
    auto it = animations_.find(animation_name);
    if (it == animations_.end())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' not found in the controller.\n", animation_name).c_str());
        return false;
    }
    const auto erased_count = std::erase_if(it->second.timeline, [id](const auto& ev)
        {
            return ev.id == id;
        });
    return erased_count > 0;
}

engine::SamplingJob::SamplingJob(const ozz::animation::Skeleton& skeleton)
    : context_(ozz::make_unique<ozz::animation::SamplingJob::Context>(skeleton.num_joints()))
    , output_(skeleton.joint_rest_poses().begin(), skeleton.joint_rest_poses().end())
{
}


bool engine::SamplingJob::is_playing() const
{
    return animation_ != nullptr;
}

void engine::SamplingJob::start(const ozz::animation::Animation* anim, const AnimationTimeline* timeline)
{
    assert(anim != nullptr);
    animation_ = anim;
    animation_timeline_ = timeline;
    time_ = 0.0f;
    timeline_next_event_id_ = 0;
}

std::string engine::SamplingJob::get_name() const
{
    assert(animation_ != nullptr);
    return animation_->name();
}

bool engine::SamplingJob::update(float dt, bool looping)
{
    ENGINE_PROFILE_SECTION;
    if (!is_playing())
    {
        return false;
    }
    assert(animation_ != nullptr);
    assert(animation_timeline_ != nullptr);
    assert(dt != 0.0f);

    const float duration = animation_->duration();
    auto fire_events = [&]()
    {
        while (timeline_next_event_id_ < animation_timeline_->size())
        {
            const auto& current_ev = animation_timeline_->at(timeline_next_event_id_);
            if (time_ < current_ev.ev.trigger_time)
            {
                break;
            }

            ++timeline_next_event_id_;
            engine_animation_event_info_t info{};
            info.current_time = time_;
            current_ev.ev.fn_ptr(&info, current_ev.ev.user_data);
        }
    };

    float remaining_time = dt / 1000.0f; // time is in seconds
    if (looping)
    {
        while (remaining_time > 0.0f)
        {
            const float segment_time = std::min(remaining_time, duration - time_);
            time_ += segment_time;
            remaining_time -= segment_time;
            fire_events();

            if (time_ >= duration)
            {
                time_ = 0.0f;
                timeline_next_event_id_ = 0;
            }
        }
    }
    else
    {
        time_ += remaining_time;
        fire_events();
    }

    const auto time_ratio = std::min(1.0f, time_ / duration);
    ozz::animation::SamplingJob sampling_job;
    sampling_job.animation = animation_;
    sampling_job.context = context_.get();
    sampling_job.ratio = time_ratio;
    sampling_job.output = ozz::make_span(output_);
    if (!sampling_job.Run()) 
    {
        log::log(log::LogLevel::eError, std::format("Animation playback '{}' has failed sampling jobl.\n", animation_->name()).c_str());
        return false;
    }
    // reset animation
    if (time_ratio == 1.0f)
    {
        reset();
    }
    return true;
}

void engine::SamplingJob::reset()
{
    time_ = 0.0f;
    animation_ = nullptr;
    animation_timeline_ = nullptr;
    timeline_next_event_id_ = 0;
}

ozz::span<ozz::math::SoaTransform> engine::SamplingJob::get_output()
{
    return ozz::make_span(output_);
}
