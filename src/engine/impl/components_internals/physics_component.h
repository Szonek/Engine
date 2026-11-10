#pragma once
#include <cstdint>
#include <memory>

namespace engine
{
    struct physcic_internal_component_t
    {
        class btCollisionShape* collision_shape = nullptr;
        class btRigidBody* rigid_body = nullptr;
        // a static body moved by its transform is reported once
        bool reported_moving_static = false;
        // what the shape reads in place (a heightfield's heights), kept alive as long as the shape
        std::shared_ptr<const void> shape_data;
    };

    // On a game object whose collider is a heightfield: the terrain and the revision of its heights the body was
    // created from (0: none, the terrain wasn't there), so the body is created again when they change.
    struct heightfield_body_internal_component_t
    {
        std::uint32_t terrain = 0;
        std::uint64_t revision = 0;
    };
}
