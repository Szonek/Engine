#include "vertex_skinning.h"
#include "graphics.h"
#include "logger.h"
#include "math_helpers.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>

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
    new_joint.init_trs.translation = glm::make_vec3(j.init_translate);
    new_joint.init_trs.rotation = glm::make_quat(j.init_rotation_quaternion);
    new_joint.init_trs.scale = glm::make_vec3(j.init_scale);
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

engine::engine_skin_internal_component_t engine::Skin::initalize_skin_component() const
{
    engine_skin_internal_component_t ret{};
    ret.bone_trs.resize(joints_.size());
    ret.bone_animation_transform.resize(joints_.size());

    for (const auto& [idx, joint] : joints_)
    {
        ret.bone_trs.at(idx) = joint.init_trs;
    }

    return ret;
}

std::vector<glm::mat4> engine::Skin::compute_transform(std::span<const TRS> bones_trs) const
{
    std::vector<glm::mat4> ret(joints_.size());
    // combine the transforms with the parent's transforms

    for (const auto& [idx, joint] : joints_)
    {
        ret[idx] = compute_model_matrix(bones_trs[idx]);

        if (joint.parent != invalid_joint_idx)
        {
            assert(joint.parent < idx); // parent index has to be smaller than joint index, because we need to gauratnee that parent trnasformation was already computed!
            ret[idx] = ret[joint.parent] * ret[idx];
        }
    }

    // pre-multiply with inverse bind matrices
    for (const auto& [idx, joint] : joints_)
    {
        ret[idx] *= joint.inverse_bind_matrix;
    }
    return ret;
}
