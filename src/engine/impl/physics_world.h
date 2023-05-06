#pragma once
#include "engine.h"

#include <btBulletDynamicsCommon.h>

#include <vector>
#include <memory>
#include <array>
#include <span>

class PhysicsWorld
{
public:
    struct physcic_internal_component_t
    {
        btCollisionShape* collision_shape = nullptr;
        btRigidBody* rigid_body = nullptr;
    };
public:
    PhysicsWorld();


    physcic_internal_component_t create_rigid_body(const engine_collider_component_t& collider,
        const engine_rigid_body_component_t& rigid_body, const engine_tranform_component_t& transform, std::int32_t body_index);


    void update(float dt);

    const std::vector<engine_collision_info_t>& get_collisions();

    void set_gravity(std::span<const float> g);

private:
    std::unique_ptr<btDefaultCollisionConfiguration> collision_config_;
    std::unique_ptr<btCollisionDispatcher> dispatcher_;
    std::unique_ptr<btBroadphaseInterface> overlapping_pair_cache_;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver_;

    std::unique_ptr<btDiscreteDynamicsWorld> dynamics_world_;

    std::vector<engine_collision_info_t> collisions_info_buffer_;
    std::vector<engine_collision_contact_point_t> collisions_contact_points_buffer_;
};