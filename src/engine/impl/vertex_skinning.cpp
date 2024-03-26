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

void engine::Skin::compute_transform(std::vector<glm::mat4>& inout_data, const glm::mat4& ltw) const
{
    // combine the transforms with the parent's transforms
    for (const auto& [idx, joint] : joints_)
    {
        if (joint.parent != invalid_joint_idx)
        {
            assert(joint.parent < idx); // parent index has to be smaller than joint index, because we need to gauratnee that parent trnasformation was already computed!
            inout_data[idx] = inout_data[joint.parent] * inout_data[idx];
        }
    }

    // pre-multiply with inverse bind matrices
    for (const auto& [idx, joint] : joints_)
    {
        inout_data[idx] *= joint.inverse_bind_matrix;
    }
}
