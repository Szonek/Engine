#include "vertex_skinning.h"
#include "graphics.h"
#include "logger.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <fmt/format.h>

namespace
{
inline engine::SkinJointDesc to_skin_desc(const engine_skin_joint_desc_t& j)
{
    engine::SkinJointDesc new_joint{};
    new_joint.idx = j.idx;
    new_joint.parent = engine::invalid_joint_idx; // leave as invalid;
    new_joint.inverse_bind_matrix = glm::make_mat4(j.inverse_bind_mat);
    for (std::uint32_t i = 0; i < j.children_count; i++)
    {
        new_joint.childrens.push_back(j.children[i]);
    }
    return new_joint;
}
}

engine::Skin::Skin(std::span<const engine_skin_joint_desc_t> joints)
{
    // create joints and set their children
    std::for_each(joints.begin(), joints.end(), [this](const engine_skin_joint_desc_t& j) 
        {
            joints_[j.idx] = to_skin_desc(j); 
        });
    // set parents fo all the nodes
    // joint without parent will keep engine::SkinJointDesc::invalid_joint (-1) value
    std::for_each(joints_.begin(), joints_.end(), [this](const auto& joint)
        {
            for (const auto& c : joint.second.childrens)
            {
                assert(joints_[c].parent == invalid_joint_idx && "Multiple parents not supported!");
                joints_[c].parent = joint.second.idx;
            }        
        });

    // find root joint
    for (const auto& [idx, joint] : joints_)
    {
        if (joint.parent == invalid_joint_idx)
        {
            assert(root_idx_ == invalid_joint_idx && "Multiple root joints not supproted!");
            root_idx_ = idx;
        }
    }
}

void engine::Skin::compute_transform(std::vector<glm::mat4>& inout_data, const glm::mat4& world_transform) const
{
    const auto inverse_world_transform = glm::inverse(world_transform);
    //https://stackoverflow.com/questions/64745393/gltf-are-bone-matrices-specified-in-local-or-model-space
    inout_data[0] = inout_data[0] * joints_.at(0).inverse_bind_matrix;

    auto a = glm::mat4(1.0f);
    a = glm::translate(a, glm::vec3(0.0f, 1.0f, 0.0f));

    inout_data[1] = glm::inverse(a) * inout_data[1] * joints_.at(1).inverse_bind_matrix;

    //inout_data[0] = inout_data[1];
    //for (const auto& j : joints_)
    //{
    //    inout_data[j.first] *= j.second.inverse_bind_matrix;
    //}
}
