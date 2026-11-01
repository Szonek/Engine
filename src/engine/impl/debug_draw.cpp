#include "debug_draw.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

void engine::DebugDrawList::add_line(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color)
{
    lines_.push_back(DebugDrawLine{ from, to, color, owner_ });
}

void engine::DebugDrawList::add_box(const glm::mat4& transform, const glm::vec3& half_extents, const glm::vec3& color)
{
    // corner i has the sign of half_extents' axis a negative when bit a of i is 0
    std::array<glm::vec3, 8> corners{};
    for (std::size_t i = 0; i < corners.size(); i++)
    {
        const glm::vec3 local(
            (i & 1) ? half_extents.x : -half_extents.x,
            (i & 2) ? half_extents.y : -half_extents.y,
            (i & 4) ? half_extents.z : -half_extents.z);
        corners[i] = glm::vec3(transform * glm::vec4(local, 1.0f));
    }
    // an edge joins two corners differing in one bit
    for (std::size_t i = 0; i < corners.size(); i++)
    {
        for (std::size_t bit = 1; bit < corners.size(); bit <<= 1)
        {
            if ((i & bit) == 0)
            {
                add_line(corners[i], corners[i | bit], color);
            }
        }
    }
}

void engine::DebugDrawList::add_circle(const glm::vec3& center, const glm::vec3& normal, float radius, const glm::vec3& color)
{
    const auto length = glm::length(normal);
    if (!(length > 0.0f))
    {
        return;
    }
    const auto n = normal / length;
    // any axis not parallel to the normal gives the circle's plane
    const auto helper = std::abs(n.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    const auto u = glm::normalize(glm::cross(n, helper)) * radius;
    const auto v = glm::cross(n, u);
    auto point = [&](std::uint32_t i)
    {
        const auto angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
        return center + u * std::cos(angle) + v * std::sin(angle);
    };
    for (std::uint32_t i = 0; i < CIRCLE_SEGMENTS; i++)
    {
        add_line(point(i), point(i + 1), color);
    }
}

void engine::DebugDrawList::add_sphere(const glm::vec3& center, float radius, const glm::vec3& color)
{
    add_circle(center, glm::vec3(1.0f, 0.0f, 0.0f), radius, color);
    add_circle(center, glm::vec3(0.0f, 1.0f, 0.0f), radius, color);
    add_circle(center, glm::vec3(0.0f, 0.0f, 1.0f), radius, color);
}

std::vector<engine::DebugDrawLine> engine::DebugDrawList::take_lines()
{
    last_owners_.clear();
    for (const auto& line : lines_)
    {
        if (line.owner != 0)
        {
            last_owners_.push_back(line.owner);
        }
    }
    std::ranges::sort(last_owners_);
    const auto duplicates = std::ranges::unique(last_owners_);
    last_owners_.erase(duplicates.begin(), duplicates.end());
    owner_ = 0;
    return std::exchange(lines_, {});
}

std::uint32_t engine::pick_game_object(std::span<const std::uint32_t> ids, std::uint32_t width, std::uint32_t height,
    std::uint32_t center_x, std::uint32_t center_y, std::span<const std::uint32_t> owners)
{
    if (center_x >= width || center_y >= height || ids.size() < static_cast<std::size_t>(width) * height)
    {
        return 0;
    }
    std::uint32_t nearest_owner = 0;
    auto nearest_distance = std::numeric_limits<std::int64_t>::max();
    for (std::uint32_t y = 0; y < height; y++)
    {
        for (std::uint32_t x = 0; x < width; x++)
        {
            const auto id = ids[static_cast<std::size_t>(y) * width + x];
            if (id == 0 || !std::ranges::binary_search(owners, id))
            {
                continue;
            }
            const auto dx = static_cast<std::int64_t>(x) - center_x;
            const auto dy = static_cast<std::int64_t>(y) - center_y;
            if (dx * dx + dy * dy < nearest_distance)
            {
                nearest_distance = dx * dx + dy * dy;
                nearest_owner = id;
            }
        }
    }
    return nearest_owner != 0 ? nearest_owner : ids[static_cast<std::size_t>(center_y) * width + center_x];
}
