#pragma once
#include "components/collider_component.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;

namespace engine
{
// what a triangle of the nav mesh input contributes
enum class NavArea : std::uint8_t
{
    // walkable where its slope is within the agent's max slope
    eWalkable = 0,
    // an obstacle: never walkable
    eNotWalkable,
};

struct NavMeshBuildSettings
{
    float agent_radius = 0.3f;
    float agent_height = 1.0f;
    float agent_max_slope = 45.0f;  // degrees
    float agent_max_climb = 0.2f;
    float cell_size = 0.1f;
    float cell_height = 0.05f;
};

// Triangles in world space, the input of a nav mesh build. Shapes are closed and convex, so their triangles are wound
// with outward normals (Recast's slope test relies on it).
class NavMeshInput
{
public:
    // the collider's shapes, placed by the world matrix of its game object (boxes: 12 triangles, spheres: a coarse
    // triangulation, compounds: each child)
    void add_collider(const engine_collider_component_t& collider, const glm::mat4& world, NavArea area);

    const std::vector<float>& get_vertices() const { return vertices_; }
    const std::vector<std::int32_t>& get_triangles() const { return triangles_; }
    const std::vector<NavArea>& get_triangle_areas() const { return areas_; }
    std::size_t get_triangle_count() const { return areas_.size(); }

private:
    void add_box(const glm::mat4& world, const glm::vec3& half_extents, NavArea area);
    void add_sphere(const glm::mat4& world, float radius, NavArea area);
    // a triangle of the convex shape with the given center, wound so its normal points away from the center
    void add_convex_shape_triangle(const glm::vec3& center, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, NavArea area);

    std::vector<float> vertices_;
    std::vector<std::int32_t> triangles_;
    std::vector<NavArea> areas_;
};

// A Recast nav mesh (one tile) with a Detour query. Built from a NavMeshInput; immutable afterwards.
class NavMesh
{
public:
    // nullptr when nothing is walkable or the build fails (logged)
    static std::unique_ptr<NavMesh> build(const NavMeshInput& input, const NavMeshBuildSettings& settings);

    NavMesh(const NavMesh&) = delete;
    NavMesh(NavMesh&&) = delete;
    NavMesh& operator=(const NavMesh&) = delete;
    NavMesh& operator=(NavMesh&&) = delete;
    ~NavMesh();

    struct Path
    {
        // the corners, the first one being the start
        std::vector<glm::vec3> points;
        // the end isn't reachable: the path ends at the reachable point closest to it
        bool partial = false;
    };

    // the closest point on the nav mesh within the box of `search_half_extents` around `point`
    std::optional<glm::vec3> find_nearest_point(const glm::vec3& point, const glm::vec3& search_half_extents) const;
    // The string-pulled path from start to end, both snapped onto the nav mesh (searched within `search_half_extents`).
    // nullopt when either point is off the nav mesh. At most max_points (> 0): a longer path is cut off (its beginning is
    // returned).
    std::optional<Path> find_path(const glm::vec3& start, const glm::vec3& end, const glm::vec3& search_half_extents, std::size_t max_points) const;

    const dtNavMesh& get_detour_nav_mesh() const { return *nav_mesh_; }

private:
    NavMesh(dtNavMesh* nav_mesh, dtNavMeshQuery* query);

    dtNavMesh* nav_mesh_ = nullptr;
    dtNavMeshQuery* query_ = nullptr;
};
}  // namespace engine
