#pragma once
#include "engine.h"
#include "math_helpers.h"

#include <cstdint>
#include <vector>
#include <map>
#include <span>
#include <string_view>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace engine
{
using JoinTypeIdx = std::int32_t;
constexpr static const JoinTypeIdx invalid_joint_idx = -1;

struct SkinJointDesc
{
    JoinTypeIdx idx = invalid_joint_idx;
    JoinTypeIdx parent = invalid_joint_idx;
    std::vector<JoinTypeIdx> childrens{};
    glm::mat4 inverse_bind_matrix{ 1.0f };   

    TRS init_trs;
};

struct engine_skin_internal_component_t
{
    std::vector<TRS> bone_trs;
    std::vector<glm::mat4> bone_animation_transform;
};

class Skin
{
public:
    Skin() = default;
    Skin(std::span<const engine_skin_joint_desc_t> joints);
    Skin(const Skin&) = delete;
    Skin(Skin&& rhs) noexcept = default;
    Skin& operator=(const Skin& rhs) = delete;
    Skin& operator=(Skin&& rhs)  noexcept = default;
    ~Skin() = default;

    engine_skin_internal_component_t initalize_skin_component() const;
    std::vector<glm::mat4> compute_transform(std::span<const TRS> bones_trs) const;

private:
    std::map<JoinTypeIdx, SkinJointDesc> joints_;
    JoinTypeIdx root_idx_ = invalid_joint_idx;
};

}