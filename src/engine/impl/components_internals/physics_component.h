#pragma once

namespace engine
{
    struct physcic_internal_component_t
    {
        class btCollisionShape* collision_shape = nullptr;
        class btRigidBody* rigid_body = nullptr;
        // a static body moved by its transform is reported once
        bool reported_moving_static = false;
    };
}