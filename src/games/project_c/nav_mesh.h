#pragma once

#include <unordered_map>

#include <glm/glm.hpp>

namespace project_c
{
using NavMeshNodeIdx = std::int32_t;
constexpr inline NavMeshNodeIdx invalid_node_idx = -1;
using NavMeshEdgeCost = std::uint32_t;
using NavMeshPosition3D = glm::vec3;
using NavMeshNodeCenterOfMass = glm::vec3;
using NavMeshNodeHalfSize = glm::vec3;

class NavMeshNode
{  
public:
    NavMeshNode(NavMeshNodeIdx my_idx, NavMeshNodeCenterOfMass&& center, NavMeshNodeHalfSize&& size);
    ~NavMeshNode() = default;
    NavMeshNode(NavMeshNode&& rhs) noexcept = default;
    NavMeshNode(const NavMeshNode& rhs) = default;
    NavMeshNode& operator=(const NavMeshNode& rhs) = default;
    NavMeshNode& operator=(NavMeshNode&& rhs) noexcept = default;

    inline NavMeshNodeIdx get_idx() const { return idx_; }
    inline NavMeshNodeCenterOfMass get_center() const { return center_; }
    inline NavMeshNodeHalfSize get_size() const { return size_; }

    void add_edge(NavMeshNodeIdx node_idx, NavMeshEdgeCost cost);
    inline const std::unordered_map<NavMeshNodeIdx, NavMeshEdgeCost>& get_edges() const { return edges_; }
private:
    NavMeshNodeCenterOfMass center_;
    NavMeshNodeHalfSize size_;
    NavMeshNodeIdx idx_;
    std::unordered_map<NavMeshNodeIdx, NavMeshEdgeCost> edges_;
};

class NavMesh
{
public:
    NavMesh();
    NavMesh(const NavMesh& rhs) = delete;
    NavMesh(NavMesh&& rhs) noexcept = default;
    NavMesh& operator=(const NavMesh& rhs) = delete;
    NavMesh& operator=(NavMesh&& rhs) noexcept = default;
    ~NavMesh() = default;

    NavMeshNodeIdx add_node(NavMeshNodeCenterOfMass&& center, NavMeshNodeHalfSize&& size);
    void add_edge(NavMeshNodeIdx node_idx_1, NavMeshNodeIdx node_idx_2, NavMeshEdgeCost cost);

    const NavMeshNode& get_node(NavMeshNodeIdx idx) const;
    NavMeshNodeIdx get_node_idx(const NavMeshPosition3D& pos) const;

private:
    std::vector<NavMeshNode> nodes_;
};

class NavMeshPathFinder
{
public:
    struct Path
    {
        std::vector<NavMeshNodeIdx> nodes;
    };
    struct PathFromStartToEnd : public Path
    {
    };
public:
    NavMeshPathFinder() = default;
    NavMeshPathFinder(const NavMeshPathFinder& rhs) = delete;
    NavMeshPathFinder(NavMeshPathFinder&& rhs) noexcept = default;
    NavMeshPathFinder& operator=(const NavMeshPathFinder& rhs) = delete;
    NavMeshPathFinder& operator=(NavMeshPathFinder&& rhs) noexcept = default;
    ~NavMeshPathFinder() = default;

    static PathFromStartToEnd find_path(const NavMesh& mesh, NavMeshNodeIdx start, NavMeshNodeIdx end);
};


}  // namespace project_c