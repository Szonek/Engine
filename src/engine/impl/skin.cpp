#include "skin.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include <stdexcept>

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
        if (skin_desc.inverse_bind_matrix_map.find(child->index) == skin_desc.inverse_bind_matrix_map.end())
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
}