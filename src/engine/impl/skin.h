#pragma once

#include "mesh_defs.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/animation/runtime/sampling_job.h"

namespace engine
{

class Skin
{
public:
    Skin(const SkinDesc& desc, const ModelNodeDesc& root);

    std::string get_name() const;

private:
    std::string name_;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton_;

    // Sampling context.
    ozz::animation::SamplingJob::Context context_;

    // Buffer of local transforms as sampled from animation_.
    ozz::vector<ozz::math::SoaTransform> locals_;

    // Buffer of model space matrices.
    ozz::vector<ozz::math::Float4x4> models_;
};
}