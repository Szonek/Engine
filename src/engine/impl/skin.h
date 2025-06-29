#pragma once

#include "mesh_defs.h"
#include "ozz/animation/offline/raw_skeleton.h"

namespace engine
{

class Skin
{
public:
    Skin(const SkinDesc& desc, const std::vector<const ModelNodeDesc*>& root)
    {
        if (root.size() != 1)
        {
            throw std::runtime_error("Skin must have exactly one root node. ToDo: implementation can be extended to support multi-root skins. Implement it if needed.");
        }
        ozz::animation::offline::RawSkeleton raw_skeleton;
        raw_skeleton.roots.resize(1);

        ozz::animation::offline::RawSkeleton::Joint& root_joint = raw_skeleton.roots[0];
    }

private:

};
}