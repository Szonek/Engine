#pragma once

#include "mesh_defs.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/memory/unique_ptr.h"

namespace engine
{

class Skin
{
public:
    Skin(const SkinDesc& desc, const ModelNodeDesc& root);

private:
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton_;
};
}