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

#include <stdexcept>
#include <format>

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

engine::AnimationController::AnimationController(Skin* skin)
    : skin_(skin)
{
    ENGINE_PROFILE_SECTION;
    if (!skin_)
    {
        throw std::invalid_argument("Skin pointer cannot be null!");
    }
    // add default layer
    assert(add_layer(0, 1.0f));
}

bool engine::AnimationController::add_animation(const AnimationClipDesc& animation_clip)
{
    ENGINE_PROFILE_SECTION;
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
    raw_animation.tracks.resize(skin_->skeleton_->num_joints());

    for (const auto& channel : animation_clip.channels)
    {
        const auto joint_idx = ozz::animation::FindJoint(*skin_->skeleton_, channel.joint_name.c_str());
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
    assert(res);
    AnimationData ad{};
    ozz::animation::offline::AnimationBuilder builder;
    ad.override = builder(raw_animation);
    ad.additive = builder(raw_animation_additive);
    animations_.emplace(raw_animation.name, std::move(ad));
    return true;
}

void engine::AnimationController::update(float dt)
{
    ENGINE_PROFILE_SECTION;

    ozz::vector<ozz::animation::BlendingJob::Layer> executed_layers;
    executed_layers.reserve(layers_.size());

    ozz::vector<ozz::animation::BlendingJob::Layer> executed_additive_layers;
    executed_additive_layers.reserve(layers_.size());

    for (auto& [_, layer] : layers_)
    {
        const bool has_played_a = layer.animation_a.update(dt);
        const bool has_played_b = layer.animation_b.update(dt);
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
        blend_job.rest_pose = skin_->skeleton_->joint_rest_poses();
        blend_job.output = ozz::make_span(layer.output);

        // Blends.
        if (!blend_job.Run()) 
        {
            log::log(log::LogLevel::eError, std::format("Failed to update animation controller for {}.\n", skin_->get_name()).c_str());
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
    ozz::vector<ozz::math::SoaTransform> final_output(skin_->skeleton_->num_soa_joints());
    ozz::animation::BlendingJob blend_job;
    blend_job.threshold = ozz::animation::BlendingJob().threshold;
    blend_job.layers = ozz::make_span(executed_layers);
    blend_job.additive_layers = ozz::make_span(executed_additive_layers);
    blend_job.rest_pose = skin_->skeleton_->joint_rest_poses();
    blend_job.output = ozz::make_span(final_output);

    // Blends.
    if (!blend_job.Run())
    {
        log::log(log::LogLevel::eError, std::format("Failed blend cross-layer result for skin: {}.\n", skin_->get_name()).c_str());
        throw std::runtime_error("Failed blend cross-layer result.");
    }

    // compute model matrices which will be used during skinning
    ozz::animation::LocalToModelJob ltm_job{};
    ltm_job.skeleton = skin_->skeleton_.get();
    ltm_job.input = ozz::make_span(final_output);
    ltm_job.output = ozz::make_span(skin_->models_);

    if (!ltm_job.Run())
    {
        log::log(log::LogLevel::eError, std::format("Failed to convert local to model space for skin: {}.\n", skin_->get_name()).c_str());
        throw std::runtime_error("Failed to convert local to model space for skin.");
    }
}

bool engine::AnimationController::add_layer(std::size_t id, float weight)
{
    if (has_layer(id))
    {
        return false;
    }

    layers_[id] = AnimationLayer(id, weight, skin_->skeleton_->num_joints(), LayerBlendMode::eOverride);
    
    return true;
}

bool engine::AnimationController::remove_layer(std::size_t id)
{
    return layers_.erase(id) != 0;
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

bool engine::AnimationController::set_layer_mode(std::size_t id, LayerBlendMode mode)
{
    if (!has_layer(id))
    {
        return false;
    }
    layers_.at(id).mode = mode;
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
    
    const auto& animation = layers_.at(layer_id).mode == LayerBlendMode::eAdditive ? it->second.additive : it->second.override;
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

    const auto& animation = layers_.at(layer_id).mode == LayerBlendMode::eAdditive ? it->second.additive : it->second.override;
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

float engine::AnimationController::get_duration(const std::string& animation_name) const
{
    auto it = animations_.find(animation_name);
    if (it == animations_.end())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' not found in the controller.\n", animation_name).c_str());
        return 0.0f;
    }
    return it->second.override->duration();
}

std::pair<bool, std::uint32_t> engine::AnimationController::add_event(const std::string& animation_name, const engine_animation_event_t& ev)
{
    auto it = animations_.find(animation_name);
    if (it == animations_.end())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' not found in the controller.\n", animation_name).c_str());
        return { false, -1 };
    }
    static std::uint32_t uuid = 0; // ToDo: this id mechanism is not ideal, improve it
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

engine::SamplingJob::SamplingJob(std::size_t num_joints)
    : context_(ozz::make_unique<ozz::animation::SamplingJob::Context>(num_joints))
    , output_((num_joints + 3)/4)  // ToDo: just call get_num_soa_joints from skeeleton?
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
}

std::string engine::SamplingJob::get_name() const
{
    assert(animation_ != nullptr);
    return animation_->name();
}

bool engine::SamplingJob::update(float dt)
{
    ENGINE_PROFILE_SECTION;
    if (!is_playing())
    {
        return false;
    }
    assert(animation_ != nullptr);
    assert(animation_timeline_ != nullptr);
    assert(dt != 0.0f);

    time_ += (dt / 1000.0f); // time is in seconds

    // 1. Loop over events, beacuse we can have multiple events triggering at the same time
    // 2. Vector of events has to be sorted!
    for(int i = timeline_next_event_id_; i < animation_timeline_->size(); i++)
    {
        const auto& current_ev = animation_timeline_->at(timeline_next_event_id_);
        if (time_ >= current_ev.ev.trigger_time)
        {
            timeline_next_event_id_++;
            engine_animation_event_info_t info{};
            info.current_time = time_;
            current_ev.ev.fn_ptr(&info, current_ev.ev.user_data);
        }
    }


    const auto time_ratio = std::min(1.0f, time_ / animation_->duration());
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
