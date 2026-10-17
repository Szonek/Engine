#include "skin.h"
#include "logger.h"
#include "math_helpers.h"
#include "profiler.h"

#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/animation_utils.h>
#include <ozz/animation/runtime/skeleton_utils.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/base/maths/transform.h>


#include <stdexcept>
#include <format>

inline void construct_skeleton(ozz::animation::offline::RawSkeleton::Joint& joint, const engine::ModelNodeDesc& node_desc, const engine::SkinDesc& skin_desc)
{
    joint.name = node_desc.name.c_str();
    joint.transform.translation = ozz::math::Float3(node_desc.translation.x, node_desc.translation.y, node_desc.translation.z);
    joint.transform.rotation = ozz::math::Quaternion(node_desc.rotation.x, node_desc.rotation.y, node_desc.rotation.z, node_desc.rotation.w);
    joint.transform.scale = ozz::math::Float3(node_desc.scale.x, node_desc.scale.y, node_desc.scale.z);

    for (const auto& child : node_desc.children)
    {
        // if the child node is not part of the skin, skip it 
        // ToDo: refactor skin API to provide only joints.
        if (skin_desc.inverse_bind_matrix_map.find(child->name) == skin_desc.inverse_bind_matrix_map.end())
        {
            continue;
        }
        ozz::animation::offline::RawSkeleton::Joint child_joint;
        construct_skeleton(child_joint, *child, skin_desc);
        joint.children.push_back(std::move(child_joint));
    }
}


namespace
{
glm::mat4 to_glm(const ozz::math::Float4x4& m)
{
    return glm::mat4(
        m.cols[0].m128_f32[0], m.cols[0].m128_f32[1], m.cols[0].m128_f32[2], m.cols[0].m128_f32[3],
        m.cols[1].m128_f32[0], m.cols[1].m128_f32[1], m.cols[1].m128_f32[2], m.cols[1].m128_f32[3],
        m.cols[2].m128_f32[0], m.cols[2].m128_f32[1], m.cols[2].m128_f32[2], m.cols[2].m128_f32[3],
        m.cols[3].m128_f32[0], m.cols[3].m128_f32[1], m.cols[3].m128_f32[2], m.cols[3].m128_f32[3]);
}
}  // namespace

engine::Skin::Skin(const SkinDesc& desc, const ModelNodeDesc& root)
    : skeleton_(nullptr)
{
    ENGINE_PROFILE_SECTION_N("engine::Skin::Skin(const SkinDesc& desc, const ModelNodeDesc& root)");
    ozz::animation::offline::RawSkeleton raw_skeleton;
    raw_skeleton.roots.resize(1);
    construct_skeleton(raw_skeleton.roots[0], root, desc);
    ozz::animation::offline::SkeletonBuilder builder;
    if (!raw_skeleton.Validate())
    {
        throw std::runtime_error("Invalid raw skeleton data.");
    }
    skeleton_ = builder(raw_skeleton);

    // copy inverse bind matrix map from desc into linear array
    join_remap_ = desc.joint_index_map;
    inverse_bind_matrices_.resize(desc.inverse_bind_matrix_map.size());
    for (const auto& [name, inv_bind_mtx] : desc.inverse_bind_matrix_map)
    {
        const auto joint_idx = ozz::animation::FindJoint(*skeleton_.get(), name.c_str());
        assert(joint_idx != -1);
        inverse_bind_matrices_.at(joint_idx) = inv_bind_mtx;
    }
    log::log(log::LogLevel::eTrace, std::format("Skin {} created with {} joints and {} SoA joints.\n", desc.asset_id, skeleton_->num_joints(), skeleton_->num_soa_joints()).c_str());
}

const ozz::animation::Skeleton& engine::Skin::get_skeleton() const
{
    return *skeleton_;
}

engine::SkinPose engine::Skin::make_rest_pose() const
{
    ENGINE_PROFILE_SECTION_N("engine::Skin::make_rest_pose()");
    SkinPose pose;
    pose.models.resize(skeleton_->num_joints());

    ozz::animation::LocalToModelJob ltm_job{};
    ltm_job.skeleton = skeleton_.get();
    ltm_job.input = skeleton_->joint_rest_poses();
    ltm_job.output = ozz::make_span(pose.models);
    if (!ltm_job.Run())
    {
        log::log(log::LogLevel::eError, "Failed to convert local to model space for a skin's rest pose.\n");
        throw std::runtime_error("Failed to convert local to model space for skin.");
    }
    compute_skinning_matrices(pose);
    return pose;
}

glm::mat4 engine::Skin::get_model_matrix_for_joint(const SkinPose& pose, std::string_view joint_name) const
{
    ENGINE_PROFILE_SECTION_N("engine::Skin::get_model_matrix_for_joint(std::string_view joint_name)");
    const auto joint_id = ozz::animation::FindJoint(*skeleton_, joint_name.data());
    if (joint_id < 0 || joint_id >= static_cast<int>(pose.models.size()))
    {
        log::log(log::LogLevel::eError, std::format("Joint '{}' not found in skin.\n", joint_name).c_str());
        return glm::mat4(1.0f);
    }
    return to_glm(pose.models[joint_id]);
}

void engine::Skin::compute_skinning_matrices(SkinPose& pose) const
{
    ENGINE_PROFILE_SECTION_N("engine::Skin::compute_skinning_matrices()");
    pose.skinning_matrices.resize(skeleton_->num_joints());
    for (int i = 0; i < skeleton_->num_joints(); ++i)
    {
        const std::string joint_name = skeleton_->joint_names()[i];
        pose.skinning_matrices[join_remap_.at(joint_name)] = to_glm(pose.models[i]) * inverse_bind_matrices_[i];
    }
}
