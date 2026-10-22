#pragma once
#include "../skin.h"

namespace engine
{
// The pose of the skin instance owned by this game object (the one with the skin component). Created in the skin's
// rest pose when the skin component gets its skin; written by the animation controller on the same game object.
struct skin_pose_internal_component_t
{
    SkinPose pose;
};
}  // namespace engine
