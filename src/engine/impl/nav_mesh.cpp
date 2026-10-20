#include "nav_mesh.h"
#include "logger.h"
#include "profiler.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>

#include <fmt/format.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <chrono>
#include <cmath>

namespace
{
// every polygon gets this flag; the default query filter only accepts polygons with a flag set
constexpr unsigned short POLY_FLAG_WALK = 0x01;
constexpr int MAX_VERTS_PER_POLY = 6;
constexpr int MAX_PATH_POLYS = 512;
constexpr int MAX_QUERY_NODES = 2048;
constexpr std::int32_t SPHERE_SEGMENTS = 8;
constexpr std::int32_t SPHERE_RINGS = 6;

template<typename T, void (*Free)(T*)>
struct RecastDeleter
{
    void operator()(T* ptr) const { Free(ptr); }
};
using HeightfieldPtr = std::unique_ptr<rcHeightfield, RecastDeleter<rcHeightfield, rcFreeHeightField>>;
using CompactHeightfieldPtr = std::unique_ptr<rcCompactHeightfield, RecastDeleter<rcCompactHeightfield, rcFreeCompactHeightfield>>;
using ContourSetPtr = std::unique_ptr<rcContourSet, RecastDeleter<rcContourSet, rcFreeContourSet>>;
using PolyMeshPtr = std::unique_ptr<rcPolyMesh, RecastDeleter<rcPolyMesh, rcFreePolyMesh>>;
using PolyMeshDetailPtr = std::unique_ptr<rcPolyMeshDetail, RecastDeleter<rcPolyMeshDetail, rcFreePolyMeshDetail>>;

// Recast reports its errors through the context
class RecastLogContext : public rcContext
{
public:
    RecastLogContext()
        : rcContext(true)
    {
    }

protected:
    void doLog(const rcLogCategory category, const char* msg, const int len) override
    {
        if (category == RC_LOG_ERROR || category == RC_LOG_WARNING)
        {
            engine::log::log(engine::log::LogLevel::eError, fmt::format("[Recast] {}\n", std::string_view(msg, len)));
        }
    }
};

glm::vec3 transform_point(const glm::mat4& m, const glm::vec3& p)
{
    return glm::vec3(m * glm::vec4(p, 1.0f));
}

glm::mat4 child_collider_matrix(const engine_child_collider_t& child)
{
    const glm::vec3 position(child.transform[0], child.transform[1], child.transform[2]);
    const auto rotation = glm::make_quat(child.rotation_quaternion);
    return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
}
}  // namespace

void engine::NavMeshInput::add_collider(const engine_collider_component_t& collider, const glm::mat4& world, NavArea area)
{
    if (collider.type == ENGINE_COLLIDER_TYPE_BOX)
    {
        add_box(world, glm::make_vec3(collider.collider.box.size), area);
    }
    else if (collider.type == ENGINE_COLLIDER_TYPE_SPHERE)
    {
        add_sphere(world, collider.collider.sphere.radius, area);
    }
    else if (collider.type == ENGINE_COLLIDER_TYPE_COMPOUND)
    {
        for (const auto& child : collider.collider.compound.children)
        {
            const auto child_world = world * child_collider_matrix(child);
            if (child.type == ENGINE_COLLIDER_TYPE_BOX)
            {
                add_box(child_world, glm::make_vec3(child.collider.box.size), area);
            }
            else if (child.type == ENGINE_COLLIDER_TYPE_SPHERE)
            {
                add_sphere(child_world, child.collider.sphere.radius, area);
            }
        }
    }
}

void engine::NavMeshInput::add_box(const glm::mat4& world, const glm::vec3& half_extents, NavArea area)
{
    std::array<glm::vec3, 8> corners{};
    for (std::uint32_t i = 0; i < corners.size(); i++)
    {
        const glm::vec3 sign((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f);
        corners[i] = transform_point(world, sign * half_extents);
    }
    const auto center = transform_point(world, glm::vec3(0.0f));
    // the faces as quads of corner indices (bit 0: +x, bit 1: +y, bit 2: +z)
    constexpr std::array<std::array<std::uint32_t, 4>, 6> faces{ {
        { 0, 2, 6, 4 }, { 1, 3, 7, 5 },  // -x, +x
        { 0, 1, 5, 4 }, { 2, 3, 7, 6 },  // -y, +y
        { 0, 1, 3, 2 }, { 4, 5, 7, 6 },  // -z, +z
    } };
    for (const auto& f : faces)
    {
        add_convex_shape_triangle(center, corners[f[0]], corners[f[1]], corners[f[2]], area);
        add_convex_shape_triangle(center, corners[f[0]], corners[f[2]], corners[f[3]], area);
    }
}

void engine::NavMeshInput::add_sphere(const glm::mat4& world, float radius, NavArea area)
{
    const auto center = transform_point(world, glm::vec3(0.0f));
    auto point = [&](std::int32_t ring, std::int32_t segment)
        {
            const float theta = glm::pi<float>() * static_cast<float>(ring) / SPHERE_RINGS;
            const float phi = glm::two_pi<float>() * static_cast<float>(segment) / SPHERE_SEGMENTS;
            const glm::vec3 local(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
            return transform_point(world, local * radius);
        };
    for (std::int32_t ring = 0; ring < SPHERE_RINGS; ring++)
    {
        for (std::int32_t segment = 0; segment < SPHERE_SEGMENTS; segment++)
        {
            const auto a = point(ring, segment);
            const auto b = point(ring, segment + 1);
            const auto c = point(ring + 1, segment + 1);
            const auto d = point(ring + 1, segment);
            // the rings at the poles have one triangle per segment
            if (ring != 0)
            {
                add_convex_shape_triangle(center, a, b, c, area);
            }
            if (ring != SPHERE_RINGS - 1)
            {
                add_convex_shape_triangle(center, a, c, d, area);
            }
        }
    }
}

void engine::NavMeshInput::add_convex_shape_triangle(const glm::vec3& center, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, NavArea area)
{
    const auto first = static_cast<std::int32_t>(vertices_.size() / 3);
    for (const auto& v : { a, b, c })
    {
        vertices_.insert(vertices_.end(), { v.x, v.y, v.z });
    }
    // Recast takes the normal as (b - a) x (c - a)
    const auto normal = glm::cross(b - a, c - a);
    const auto outward = (a + b + c) / 3.0f - center;
    if (glm::dot(normal, outward) >= 0.0f)
    {
        triangles_.insert(triangles_.end(), { first, first + 1, first + 2 });
    }
    else
    {
        triangles_.insert(triangles_.end(), { first, first + 2, first + 1 });
    }
    areas_.push_back(area);
}

std::unique_ptr<engine::NavMesh> engine::NavMesh::build(const NavMeshInput& input, const NavMeshBuildSettings& settings)
{
    ENGINE_PROFILE_SECTION;
    const auto start_time = std::chrono::steady_clock::now();
    if (input.get_triangle_count() == 0)
    {
        log::log(log::LogLevel::eError, "Nav mesh build: no colliders to build from.\n");
        return nullptr;
    }
    if (settings.cell_size <= 0.0f || settings.cell_height <= 0.0f || settings.agent_height <= 0.0f || settings.agent_radius < 0.0f)
    {
        log::log(log::LogLevel::eError, "Nav mesh build: cell size, cell height and agent height must be greater than zero.\n");
        return nullptr;
    }

    const auto& vertices = input.get_vertices();
    const auto& triangles = input.get_triangles();
    const auto vertex_count = static_cast<int>(vertices.size() / 3);
    const auto triangle_count = static_cast<int>(input.get_triangle_count());

    rcConfig config{};
    config.cs = settings.cell_size;
    config.ch = settings.cell_height;
    config.walkableSlopeAngle = settings.agent_max_slope;
    config.walkableHeight = static_cast<int>(std::ceil(settings.agent_height / config.ch));
    config.walkableClimb = static_cast<int>(std::floor(settings.agent_max_climb / config.ch));
    config.walkableRadius = static_cast<int>(std::ceil(settings.agent_radius / config.cs));
    config.maxEdgeLen = static_cast<int>(12.0f / config.cs);
    config.maxSimplificationError = 1.3f;
    config.minRegionArea = 8 * 8;
    config.mergeRegionArea = 20 * 20;
    config.maxVertsPerPoly = MAX_VERTS_PER_POLY;
    config.detailSampleDist = 6.0f * config.cs;
    config.detailSampleMaxError = 1.0f * config.ch;
    rcCalcBounds(vertices.data(), vertex_count, config.bmin, config.bmax);
    rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);

    RecastLogContext context;

    // rasterize the triangles into voxels; walkable is decided by slope, unless a modifier decided it
    HeightfieldPtr heightfield(rcAllocHeightfield());
    if (!heightfield || !rcCreateHeightfield(&context, *heightfield, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch))
    {
        return nullptr;
    }
    std::vector<unsigned char> areas(triangle_count, RC_NULL_AREA);
    rcMarkWalkableTriangles(&context, config.walkableSlopeAngle, vertices.data(), vertex_count, triangles.data(), triangle_count, areas.data());
    const auto& input_areas = input.get_triangle_areas();
    for (int i = 0; i < triangle_count; i++)
    {
        if (input_areas[i] == NavArea::eWalkable)
        {
            areas[i] = RC_WALKABLE_AREA;
        }
        else if (input_areas[i] == NavArea::eNotWalkable)
        {
            areas[i] = RC_NULL_AREA;
        }
    }
    if (!rcRasterizeTriangles(&context, vertices.data(), vertex_count, triangles.data(), areas.data(), triangle_count, *heightfield, config.walkableClimb))
    {
        return nullptr;
    }

    // remove what the agent can't stand on: low ceilings, ledges
    rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *heightfield);
    rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *heightfield);
    rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *heightfield);

    // walkable surface, shrunk by the agent radius, partitioned into regions
    CompactHeightfieldPtr compact(rcAllocCompactHeightfield());
    if (!compact || !rcBuildCompactHeightfield(&context, config.walkableHeight, config.walkableClimb, *heightfield, *compact))
    {
        return nullptr;
    }
    heightfield.reset();
    if (!rcErodeWalkableArea(&context, config.walkableRadius, *compact)
        || !rcBuildDistanceField(&context, *compact)
        || !rcBuildRegions(&context, *compact, 0, config.minRegionArea, config.mergeRegionArea))
    {
        return nullptr;
    }

    // region outlines → convex polygons, with height detail
    ContourSetPtr contours(rcAllocContourSet());
    if (!contours || !rcBuildContours(&context, *compact, config.maxSimplificationError, config.maxEdgeLen, *contours))
    {
        return nullptr;
    }
    PolyMeshPtr poly_mesh(rcAllocPolyMesh());
    if (!poly_mesh || !rcBuildPolyMesh(&context, *contours, config.maxVertsPerPoly, *poly_mesh))
    {
        return nullptr;
    }
    PolyMeshDetailPtr detail_mesh(rcAllocPolyMeshDetail());
    if (!detail_mesh || !rcBuildPolyMeshDetail(&context, *poly_mesh, *compact, config.detailSampleDist, config.detailSampleMaxError, *detail_mesh))
    {
        return nullptr;
    }
    if (poly_mesh->npolys == 0)
    {
        log::log(log::LogLevel::eError, fmt::format("Nav mesh build: nothing walkable in {} triangles.\n", triangle_count));
        return nullptr;
    }

    for (int i = 0; i < poly_mesh->npolys; i++)
    {
        poly_mesh->flags[i] = POLY_FLAG_WALK;
    }

    dtNavMeshCreateParams params{};
    params.verts = poly_mesh->verts;
    params.vertCount = poly_mesh->nverts;
    params.polys = poly_mesh->polys;
    params.polyAreas = poly_mesh->areas;
    params.polyFlags = poly_mesh->flags;
    params.polyCount = poly_mesh->npolys;
    params.nvp = poly_mesh->nvp;
    params.detailMeshes = detail_mesh->meshes;
    params.detailVerts = detail_mesh->verts;
    params.detailVertsCount = detail_mesh->nverts;
    params.detailTris = detail_mesh->tris;
    params.detailTriCount = detail_mesh->ntris;
    params.walkableHeight = settings.agent_height;
    params.walkableRadius = settings.agent_radius;
    params.walkableClimb = settings.agent_max_climb;
    rcVcopy(params.bmin, poly_mesh->bmin);
    rcVcopy(params.bmax, poly_mesh->bmax);
    params.cs = config.cs;
    params.ch = config.ch;
    params.buildBvTree = true;

    unsigned char* data = nullptr;
    int data_size = 0;
    if (!dtCreateNavMeshData(&params, &data, &data_size))
    {
        log::log(log::LogLevel::eError, "Nav mesh build: creating the Detour nav mesh data failed.\n");
        return nullptr;
    }
    dtNavMesh* nav_mesh = dtAllocNavMesh();
    if (!nav_mesh || dtStatusFailed(nav_mesh->init(data, data_size, DT_TILE_FREE_DATA)))
    {
        dtFree(data);
        dtFreeNavMesh(nav_mesh);
        log::log(log::LogLevel::eError, "Nav mesh build: initializing the Detour nav mesh failed.\n");
        return nullptr;
    }
    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    if (!query || dtStatusFailed(query->init(nav_mesh, MAX_QUERY_NODES)))
    {
        dtFreeNavMeshQuery(query);
        dtFreeNavMesh(nav_mesh);
        log::log(log::LogLevel::eError, "Nav mesh build: initializing the Detour query failed.\n");
        return nullptr;
    }

    const auto build_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    log::log(log::LogLevel::eTrace, fmt::format("Nav mesh built from {} triangles in {:.1f} ms: {} polygons.\n",
        triangle_count, build_ms, poly_mesh->npolys));
    return std::unique_ptr<NavMesh>(new NavMesh(nav_mesh, query));
}

engine::NavMesh::NavMesh(dtNavMesh* nav_mesh, dtNavMeshQuery* query)
    : nav_mesh_(nav_mesh)
    , query_(query)
{
}

engine::NavMesh::~NavMesh()
{
    dtFreeNavMeshQuery(query_);
    dtFreeNavMesh(nav_mesh_);
}

std::optional<glm::vec3> engine::NavMesh::find_nearest_point(const glm::vec3& point) const
{
    const dtQueryFilter filter;
    dtPolyRef poly = 0;
    glm::vec3 nearest(0.0f);
    if (dtStatusFailed(query_->findNearestPoly(glm::value_ptr(point), QUERY_HALF_EXTENTS, &filter, &poly, glm::value_ptr(nearest))) || poly == 0)
    {
        return std::nullopt;
    }
    return nearest;
}

std::vector<glm::vec3> engine::NavMesh::find_path(const glm::vec3& start, const glm::vec3& end, std::size_t max_points) const
{
    ENGINE_PROFILE_SECTION;
    if (max_points == 0)
    {
        return {};
    }
    const dtQueryFilter filter;
    dtPolyRef start_poly = 0;
    dtPolyRef end_poly = 0;
    glm::vec3 start_on_mesh(0.0f);
    glm::vec3 end_on_mesh(0.0f);
    query_->findNearestPoly(glm::value_ptr(start), QUERY_HALF_EXTENTS, &filter, &start_poly, glm::value_ptr(start_on_mesh));
    query_->findNearestPoly(glm::value_ptr(end), QUERY_HALF_EXTENTS, &filter, &end_poly, glm::value_ptr(end_on_mesh));
    if (start_poly == 0 || end_poly == 0)
    {
        return {};
    }

    std::array<dtPolyRef, MAX_PATH_POLYS> polys{};
    int poly_count = 0;
    const auto status = query_->findPath(start_poly, end_poly, glm::value_ptr(start_on_mesh), glm::value_ptr(end_on_mesh), &filter,
        polys.data(), &poly_count, MAX_PATH_POLYS);
    // a partial path ends at the polygon closest to the end: the end isn't reachable
    if (dtStatusFailed(status) || dtStatusDetail(status, DT_PARTIAL_RESULT) || poly_count == 0 || polys[poly_count - 1] != end_poly)
    {
        return {};
    }

    std::vector<glm::vec3> points(max_points);
    int point_count = 0;
    if (dtStatusFailed(query_->findStraightPath(glm::value_ptr(start_on_mesh), glm::value_ptr(end_on_mesh), polys.data(), poly_count,
        glm::value_ptr(points[0]), nullptr, nullptr, &point_count, static_cast<int>(max_points))))
    {
        return {};
    }
    points.resize(point_count);
    return points;
}
