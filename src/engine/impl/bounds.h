#pragma once
#include <glm/glm.hpp>

#include <array>
#include <cmath>

namespace engine
{
// An axis-aligned box. Header-only and free of the graphics API: culling is unit tested (bounds_tests.cpp).
struct Aabb
{
    glm::vec3 min{ 0.0f };
    glm::vec3 max{ 0.0f };

    glm::vec3 center() const { return (min + max) * 0.5f; }
    glm::vec3 half_extents() const { return (max - min) * 0.5f; }
};

// the axis-aligned box around `box` placed by `matrix` (an affine transform)
inline Aabb transform_aabb(const Aabb& box, const glm::mat4& matrix)
{
    const auto center = glm::vec3(matrix * glm::vec4(box.center(), 1.0f));
    const auto half = box.half_extents();
    // each world axis gets the box's extents along it: the absolute values of the matrix's rows
    const glm::vec3 extents(
        std::abs(matrix[0][0]) * half.x + std::abs(matrix[1][0]) * half.y + std::abs(matrix[2][0]) * half.z,
        std::abs(matrix[0][1]) * half.x + std::abs(matrix[1][1]) * half.y + std::abs(matrix[2][1]) * half.z,
        std::abs(matrix[0][2]) * half.x + std::abs(matrix[1][2]) * half.y + std::abs(matrix[2][2]) * half.z);
    return Aabb{ center - extents, center + extents };
}

// The volume a camera sees, as six planes pointing inwards (a point is inside when every plane gives it a distance >= 0).
class Frustum
{
public:
    // from a projection * view matrix with OpenGL's clip space (z from -1 to 1): perspective and orthographic cameras,
    // and the directional light's shadow camera (Gribb & Hartmann)
    static Frustum from_view_projection(const glm::mat4& view_projection)
    {
        const auto row = [&view_projection](int index)
            {
                return glm::vec4(view_projection[0][index], view_projection[1][index], view_projection[2][index], view_projection[3][index]);
            };
        Frustum frustum;
        frustum.planes_ = {
            row(3) + row(0),  // left
            row(3) - row(0),  // right
            row(3) + row(1),  // bottom
            row(3) - row(1),  // top
            row(3) + row(2),  // near
            row(3) - row(2),  // far
        };
        for (auto& plane : frustum.planes_)
        {
            const auto length = glm::length(glm::vec3(plane));
            if (length > 0.0f)
            {
                plane /= length;
            }
        }
        return frustum;
    }

    // False only when the box is certainly outside: it is behind one of the planes. Conservative, as usual: a box
    // outside near a corner of the frustum can pass.
    bool intersects(const Aabb& world_box) const
    {
        const auto center = world_box.center();
        const auto half = world_box.half_extents();
        for (const auto& plane : planes_)
        {
            const auto normal = glm::vec3(plane);
            // the box's corner furthest along the plane's normal
            const auto reach = glm::dot(glm::abs(normal), half);
            if (glm::dot(normal, center) + plane.w + reach < 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    const std::array<glm::vec4, 6>& get_planes() const { return planes_; }

private:
    std::array<glm::vec4, 6> planes_{};
};
}  // namespace engine
