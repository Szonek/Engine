#pragma once

namespace engine
{
    struct physcic_internal_component_t
    {
        class btCollisionShape* collision_shape = nullptr;
        class btRigidBody* rigid_body = nullptr;
    };
}