#pragma once

#include "mesh_defs.h"

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/base/memory/unique_ptr.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/animation/runtime/sampling_job.h>


#include <span>

namespace engine
{

// The pose of one instance of a skin: written by its animation controller, read by skinning and joint attachments.
struct SkinPose
{
    // model space matrix of every joint
    ozz::vector<ozz::math::Float4x4> models;
    // what the skinned meshes are deformed with, in glTF joint order
    std::vector<glm::mat4> skinning_matrices;
};

// A skin asset, shared by every instance of a skinned model: the skeleton and the data that maps it to the mesh.
// It never changes after creation; the per-instance state is a SkinPose. Skins live in the application's atlas,
// named by their asset ID ("<model file>#<skin name>").
class Skin
{
public:
    Skin() = default;
    Skin(const SkinDesc& desc, const ModelNodeDesc& root);

    const ozz::animation::Skeleton& get_skeleton() const;

    // a pose standing in the skeleton's rest pose, with its skinning matrices computed
    SkinPose make_rest_pose() const;
    glm::mat4 get_model_matrix_for_joint(const SkinPose& pose, std::string_view joint_name) const;
    void compute_skinning_matrices(SkinPose& pose) const; // generally should be computed once per frame, after animations are done

private:
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton_;
    std::unordered_map<std::string, std::int32_t> join_remap_;  // required to remap computed model matrices during skinning
    std::vector<glm::mat4> inverse_bind_matrices_; // required during skinning
};
}