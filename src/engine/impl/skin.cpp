#include "skin.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"

engine::Skin::Skin(const SkinDesc& desc, const ModelNodeDesc& root)
    : skeleton_(nullptr)
{
    ozz::animation::offline::RawSkeleton raw_skeleton;
    raw_skeleton.roots.resize(1);

    ozz::animation::offline::RawSkeleton::Joint& root_joint = raw_skeleton.roots[0];
}