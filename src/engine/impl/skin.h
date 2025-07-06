#pragma once

#include "mesh_defs.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/animation/runtime/sampling_job.h"

#include <span>

namespace engine
{

class Skin
{
public:
    Skin(const SkinDesc& desc, const ModelNodeDesc& root);

    const std::string& get_name() const;

    glm::mat4 get_model_matrix_for_joint(std::string_view joint_name) const;

    void compute_skinning_matrices(); // generally should be computed once per frame, after animations are done
    std::span<const glm::mat4> get_skinning_matrices() const;

private:
    friend class AnimationController;
    std::string name_;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton_;

    // Buffer of local transforms as sampled from animation_.
    ozz::vector<ozz::math::SoaTransform> locals_;

    // Buffer of model space matrices.
    ozz::vector<ozz::math::Float4x4> models_;

    std::unordered_map<std::string, std::int32_t> join_remap_;  // required to remap computed model matrices during skinning
    std::vector<glm::mat4> inverse_bind_matrices_; // required during skinning

    std::vector<glm::mat4> skinning_matrices_;
};
}