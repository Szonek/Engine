#include "nav_mesh.h"

#include <queue>
#include <unordered_set>

project_c::NavMeshNodeIdx project_c::NavMesh::add_node(NavMeshNodeCenterOfMass&& center, NavMeshNodeHalfSize&& size)
{
    nodes_.emplace_back(project_c::NavMeshNode(nodes_.size(), std::move(center), std::move(size)));
    return nodes_.back().get_idx();
}

void project_c::NavMesh::add_edge(NavMeshNodeIdx node_idx_1, NavMeshNodeIdx node_idx_2, NavMeshEdgeCost cost)
{
    if (node_idx_1 == node_idx_2)
    {
        assert(false && "Cannot add edge to self");
        return;
    }

    if (node_idx_1 < 0 || node_idx_1 >= nodes_.size())
    {
        assert(false && "Invalid node index");
        return;
    }

    if (node_idx_2 < 0 || node_idx_2 >= nodes_.size())
    {
        assert(false && "Invalid node index");
        return;
    }

    nodes_[node_idx_1].add_edge(node_idx_2, cost);
}

const project_c::NavMeshNode& project_c::NavMesh::get_node(NavMeshNodeIdx idx) const
{
    assert(idx >= 0 && idx < nodes_.size() && "Invalid node index");
    return nodes_[idx];
}

project_c::NavMeshNodeIdx project_c::NavMesh::get_node_idx(const NavMeshPosition3D& pos) const
{
    // iterate over all nodes and check if "pos" is in bounding box of Node
    for (const auto& node : nodes_)
    {
        const auto center = node.get_center();
        const auto size = node.get_size();
        if (pos.x >= center.x - size.x && pos.x <= center.x + size.x &&
            //pos.y >= center.y - size.y && pos.y <= center.y + size.y &&
            pos.z >= center.z - size.z && pos.z <= center.z + size.z)
        {
            return node.get_idx();
        }
    }
    return invalid_node_idx;
}

project_c::NavMeshNode::NavMeshNode(NavMeshNodeIdx my_idx, NavMeshNodeCenterOfMass&& pos, NavMeshNodeHalfSize&& size)
    : idx_(my_idx)
    , center_(std::move(pos))
    , size_(std::move(size))
{
    assert(idx_ != invalid_node_idx);
}

void project_c::NavMeshNode::add_edge(NavMeshNodeIdx node_idx, NavMeshEdgeCost cost)
{
    if (edges_.find(node_idx) != edges_.end())
    {
        assert(false && "Edge already exists");
        return;
    }
    edges_.emplace(node_idx, cost);
}

project_c::NavMesh::NavMesh()
{
    nodes_.reserve(1024);
}

project_c::NavMeshPathFinder::PathFromStartToEnd project_c::NavMeshPathFinder::find_path(const NavMesh& mesh, NavMeshNodeIdx start, NavMeshNodeIdx end)
{
    constexpr bool optimization_early_exit = true;

    PathFromStartToEnd ret{};

    std::queue<NavMeshNodeIdx> frontier;
    frontier.push(start);
    std::unordered_map<NavMeshNodeIdx, NavMeshNodeIdx> came_from;
    came_from[start] = start;

    while (!frontier.empty())
    {
        const auto current = mesh.get_node(frontier.front());
        if constexpr (optimization_early_exit)
        {
            if (current.get_idx() == end)
            {
                break;
            }
        }
        frontier.pop();
        const auto edges = current.get_edges();
        for (const auto next : edges)
        {
            if (came_from.find(next.first) == came_from.end())
            {
                frontier.push(next.first);
                came_from[next.first] = current.get_idx();
            }
        }
    }

    while (end != start)
    {
        ret.nodes.push_back(end);
        end = came_from[end];
    }
    //ret.nodes.push_back(start);
    // reverse so it's from start to begin
    std::reverse(ret.nodes.begin(), ret.nodes.end());
    return ret;
}
