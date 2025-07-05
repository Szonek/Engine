#include "animation_controller.h"
#include "skin.h"
#include "logger.h"

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/runtime/skeleton_utils.h"

#include <stdexcept>
#include <format>

engine::AnimationController::AnimationController(Skin* skin)
    : skin_(skin)
{
    if (!skin_)
    {
        throw std::invalid_argument("Skin pointer cannot be null!");
    }
    context_.Resize(skin_->skeleton_->num_joints());
}

bool engine::AnimationController::add_animation(const AnimationClipDesc& animation_clip)
{
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

    ozz::animation::offline::AnimationBuilder builder;
    ozz::unique_ptr<ozz::animation::Animation> animation = builder(raw_animation);
    animations_.emplace(raw_animation.name, std::move(animation));
    return true;
}

void engine::AnimationController::update(float dt)
{
    for (auto it = jobs_.begin(); it != jobs_.end(); )
    {
        const auto should_erase = it->second.update(dt, ozz::make_span(skin_->locals_));
        if (should_erase)
        {
            it = jobs_.erase(it);
        }
        else 
        {
            ++it;
        }
    }
}

bool engine::AnimationController::play(const std::string& animation_name)
{
    auto it = animations_.find(animation_name);
    if (it == animations_.end())
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' not found in the controller.\n", animation_name).c_str());
        return false;
    }
    const auto& animation = it->second;
    if (!animation)
    {
        log::log(log::LogLevel::eError, std::format("Animation '{}' is null.\n", animation_name).c_str());
        return false;
    }
    const auto layer_id = 0;
    jobs_.emplace(layer_id, PlayBackJob(animation.get(), context_, skin_->skeleton_->num_joints()));
    return true;
}

bool engine::AnimationController::is_playing(const std::string& animation_name) const
{
    auto it = std::find_if(jobs_.begin(), jobs_.end(), [&animation_name](const auto& it)
        {
            return it.second.get_animation_name() == animation_name;
        }
    );
    return it != jobs_.end();
}

engine::PlayBackJob::PlayBackJob(ozz::animation::Animation* anim, ozz::animation::SamplingJob::Context& ctx, std::size_t num_joints)
    : animation_(anim)
    , context_(ctx)
{
    assert(animation_ != nullptr);
}


bool engine::PlayBackJob::update(float dt, ozz::span<ozz::math::SoaTransform> output)
{
    assert(animation_ != nullptr);
    assert(dt != 0.0f);
    time_ += (dt / 1000.0f); // time is in seconds
    const auto time_ratio = std::min(1.0f, time_ / animation_->duration());
    ozz::animation::SamplingJob sampling_job;
    sampling_job.animation = animation_;
    sampling_job.context = &context_;
    sampling_job.ratio = time_ratio;
    sampling_job.output = output;
    if (!sampling_job.Run()) 
    {
        log::log(log::LogLevel::eError, std::format("Animation playback '{}' has failed sampling jobl.\n", animation_->name()).c_str());
    }
    // scale delta time to seconds
    return time_ >= animation_->duration();
}
