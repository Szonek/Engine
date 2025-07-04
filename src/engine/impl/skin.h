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

    const std::string& get_name() const;

    glm::mat4 get_model_matrix_for_joint(std::string_view joint_name) const;

    std::vector<glm::mat4> get_skinning_matrices();

private:
    friend class AnimationController;
    std::string name_;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton_;

    // Buffer of local transforms as sampled from animation_.
    ozz::vector<ozz::math::SoaTransform> locals_;

    // Buffer of model space matrices.
    ozz::vector<ozz::math::Float4x4> models_;

    std::unordered_map<std::string, std::int32_t> temp_joint_index_map_;
    std::vector<glm::mat4> temp_inverse_bind_matrices_;
};
}