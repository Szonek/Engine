#pragma once
#include "mesh_defs.h"

#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/base/memory/unique_ptr.h"

#include <unordered_map>

namespace engine
{
class AnimationController
{
public:
    AnimationController(class Skin* skin);

    void add_animation(const AnimationClipDesc& animation_clip);

private:
    ozz::animation::Skeleton* skeleton_ = nullptr;
    std::unordered_map<std::string, ozz::unique_ptr<ozz::animation::Animation>> animations_;
};

} // namespace engine