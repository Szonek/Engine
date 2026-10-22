#pragma once
#include "../animation_controller.h"

#include <memory>

namespace engine
{
// The animation controller of this game object, owned here: created when its animation controller component gets an
// animation set (and the game object has a skin), destroyed with that component or the game object.
struct animation_controller_internal_component_t
{
    std::unique_ptr<AnimationController> controller;
    // the animation set it was built from: an update with the same set (and skin) only reconfigures the layers
    std::uint32_t animation_set;
};
}  // namespace engine
