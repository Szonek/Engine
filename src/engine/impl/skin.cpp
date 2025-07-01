#include "skin.h"
#include "logger.h"
#include "math_helpers.h"

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
    temp_joint_index_map_ = desc.joint_index_map;
    temp_inverse_bind_matrices_.resize(desc.inverse_bind_matrix_map.size());
    for (const auto& [name, inv_bind_mtx] : desc.inverse_bind_matrix_map)
    {
        const auto joint_idx = ozz::animation::FindJoint(*skeleton_.get(), name.c_str());
        assert(joint_idx != -1);
        temp_inverse_bind_matrices_.at(joint_idx) = inv_bind_mtx;
    }



    const int num_soa_joints = skeleton_->num_soa_joints();
    locals_.resize(num_soa_joints, ozz::math::SoaTransform::identity());
    const int num_joints = skeleton_->num_joints();
    models_.resize(num_joints);
    log::log(log::LogLevel::eTrace, std::format("Skin {} created with {} joints and {} SoA joints.\n", name_, num_joints, num_soa_joints).c_str());

    // Allocates a context that matches animation requirements.
    context_.Resize(num_joints);

    // Initialize rest pose (just in case, if there was no animation played).
    ozz::animation::LocalToModelJob ltm_job{};
    ltm_job.skeleton = skeleton_.get();
    ltm_job.input = skeleton_->joint_rest_poses();
    ltm_job.output = ozz::make_span(models_);

    if (!ltm_job.Run())
    {
        log::log(log::LogLevel::eError, std::format("Failed to convert local to model space for skin: {}.\n", name_).c_str());
        throw std::runtime_error("Failed to convert local to model space for skin.");
    }

    //if (false)
    //{
    //    ozz::animation::offline::RawAnimation raw_animation;
    //    raw_animation.duration = 1.0f;
    //    raw_animation.tracks.resize(skeleton_->num_joints());
    //    raw_animation.name = "testa_anim";
    //    if (!raw_animation.Validate())
    //    {
    //        log::log(log::LogLevel::eError, std::format("Invalid raw animation data for skin: {}.\n", name_).c_str());
    //        return false;
    //    }
    //    ozz::animation::offline::AnimationBuilder builder;
    //    ozz::unique_ptr<ozz::animation::Animation> animation = builder(raw_animation);

    //    ozz::animation::SamplingJob sampling_job{};
    //    sampling_job.animation = animation.get();
    //    sampling_job.context = &context_;
    //    sampling_job.ratio = 0.0f;
    //    sampling_job.output = ozz::make_span(locals_);

    //    assert(sampling_job.Validate());
    //    if (!sampling_job.Run())
    //    {
    //        log::log(log::LogLevel::eError, std::format("Failed to sample animation for skin: {}.\n", name_).c_str());
    //        return false;
    //    }
    //    ltm_job.input = ozz::make_span(locals_);
    //}
}

const std::string& engine::Skin::get_name() const
{
    return name_;
}

std::vector<glm::mat4> engine::Skin::get_skinning_matrices() const
{
    std::vector<glm::mat4> ret{};
    ret.resize(skeleton_->num_joints());
    for (int i = 0; i < skeleton_->num_joints(); ++i)
    {
        const auto transform = ozz::animation::GetJointLocalRestPose(*skeleton_.get(), i);
        const auto glm_translation = glm::vec3(transform.translation.x, transform.translation.y, transform.translation.z);
        const auto glm_scale = glm::vec3(transform.scale.x, transform.scale.y, transform.scale.z);
        const auto glm_rotation = glm::quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);

        const std::string joint_name = skeleton_->joint_names()[i];
        ret[temp_joint_index_map_.at(joint_name)] = compute_model_matrix(glm_translation, glm_rotation, glm_scale) * temp_inverse_bind_matrices_[i];
        //ozz::math::Float4x4::Translation
        //ret.emplace_back(glm::mat4(
        //    model.cols[0].m128_f32[1], model.cols[0].m128_f32[1], model.cols[0].m128_f32[2], model.cols[0].m128_f32[3],
        //    model.cols[1].m128_f32[1], model.cols[1].m128_f32[1], model.cols[1].m128_f32[2], model.cols[1].m128_f32[3],
        //    model.cols[2].m128_f32[1], model.cols[2].m128_f32[1], model.cols[2].m128_f32[2], model.cols[2].m128_f32[3],
        //    model.cols[3].m128_f32[1], model.cols[3].m128_f32[1], model.cols[3].m128_f32[2], model.cols[3].m128_f32[3]) * temp_inverse_bind_matrices_[i]);
    }
    return ret;
}
