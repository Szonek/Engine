#pragma once
#include "engine_string_impl_def.h"

#include <memory>

namespace engine
{
// The joint name string of a joint attachment, owned by the engine: the component's joint_name points at it for the
// component's whole life and it is destroyed with the component. A different string passed in an update is copied
// into it, never taken over (see Scene).
struct joint_attachment_internal_component_t
{
    std::unique_ptr<engine_string_t> joint_name = std::make_unique<engine_string_t>();
};
}  // namespace engine
