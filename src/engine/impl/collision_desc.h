#pragma once
#include <cstdint>
#include "math_helpers.h"

namespace engine
{

struct CollisionContactPointDesc
{
    glm::vec3 point_on_obj_a;
    glm::vec3 point_on_obj_b;
    std::int32_t lifetime;
};

struct CollisionDesc
{
    std::uint32_t object_a;
    std::uint32_t object_b;

    std::vector<CollisionContactPointDesc> contact_points;

};

} // namespace engine