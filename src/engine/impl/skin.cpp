#include "skin.h"
#include "logger.h"
#include "math_helpers.h"
#include "profiler.h"

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/animation_utils.h"
#include "ozz/animation/runtime/skeleton_utils.h"
#include "ozz/base/maths/transform.h"
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


engine::Skin::Skin(const SkinDesc& desc, const ModelNodeDesc& root)
    : skeleton_(nullptr)
    , name_(desc.name)
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

    const int num_soa_joints = skeleton_->num_soa_joints();
    locals_.resize(num_soa_joints, ozz::math::SoaTransform::identity());
    for (auto i = 0; i < num_soa_joints; i++)
    {
        locals_[i] = skeleton_->joint_rest_poses()[i];
    }
    const int num_joints = skeleton_->num_joints();
    models_.resize(num_joints);
    log::log(log::LogLevel::eTrace, std::format("Skin {} created with {} joints and {} SoA joints.\n", name_, num_joints, num_soa_joints).c_str());

    // Initialize rest pose (just in case, if there was no animation played).
    ozz::animation::LocalToModelJob ltm_job{};
    ltm_job.skeleton = skeleton_.get();
    ltm_job.input = ozz::make_span(locals_);
    ltm_job.output = ozz::make_span(models_);

    if (!ltm_job.Run())
    {
        log::log(log::LogLevel::eError, std::format("Failed to convert local to model space for skin: {}.\n", name_).c_str());
        throw std::runtime_error("Failed to convert local to model space for skin.");
    }

}

const std::string& engine::Skin::get_name() const
{
    return name_;
}

glm::mat4 engine::Skin::get_model_matrix_for_joint(std::string_view joint_name) const
{
    ENGINE_PROFILE_SECTION_N("engine::Skin::get_model_matrix_for_joint(std::string_view joint_name)");
    const auto joint_id = ozz::animation::FindJoint(*skeleton_, joint_name.data());
    const auto& model = models_[joint_id];
    return glm::mat4(
        model.cols[0].m128_f32[0], model.cols[0].m128_f32[1], model.cols[0].m128_f32[2], model.cols[0].m128_f32[3],
        model.cols[1].m128_f32[0], model.cols[1].m128_f32[1], model.cols[1].m128_f32[2], model.cols[1].m128_f32[3],
        model.cols[2].m128_f32[0], model.cols[2].m128_f32[1], model.cols[2].m128_f32[2], model.cols[2].m128_f32[3],
        model.cols[3].m128_f32[0], model.cols[3].m128_f32[1], model.cols[3].m128_f32[2], model.cols[3].m128_f32[3]);
}

void engine::Skin::compute_skinning_matrices()
{
    ENGINE_PROFILE_SECTION_N("engine::Skin::compute_skinning_matrices()");
    ozz::animation::LocalToModelJob ltm_job{};
    ltm_job.skeleton = skeleton_.get();
    ltm_job.input = ozz::make_span(locals_);
    ltm_job.output = ozz::make_span(models_);

    if (!ltm_job.Run())
    {
        log::log(log::LogLevel::eError, std::format("Failed to convert local to model space for skin: {}.\n", name_).c_str());
        throw std::runtime_error("Failed to convert local to model space for skin.");
    }

    
    skinning_matrices_.resize(skeleton_->num_joints());
    for (int i = 0; i < skeleton_->num_joints(); ++i)
    {
        const std::string joint_name = skeleton_->joint_names()[i];
        const auto& model = models_[i];
        skinning_matrices_[join_remap_.at(joint_name)] = glm::mat4(
            model.cols[0].m128_f32[0], model.cols[0].m128_f32[1], model.cols[0].m128_f32[2], model.cols[0].m128_f32[3],
            model.cols[1].m128_f32[0], model.cols[1].m128_f32[1], model.cols[1].m128_f32[2], model.cols[1].m128_f32[3],
            model.cols[2].m128_f32[0], model.cols[2].m128_f32[1], model.cols[2].m128_f32[2], model.cols[2].m128_f32[3],
            model.cols[3].m128_f32[0], model.cols[3].m128_f32[1], model.cols[3].m128_f32[2], model.cols[3].m128_f32[3]) * inverse_bind_matrices_[i];
    }
}

std::span<const glm::mat4> engine::Skin::get_skinning_matrices() const
{
    return skinning_matrices_;
}
